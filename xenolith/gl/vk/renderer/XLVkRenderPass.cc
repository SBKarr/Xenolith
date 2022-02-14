/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLVkRenderPass.h"

#include "XLGlFrameHandle.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkImageAttachment.h"
#include "XLVkDevice.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::vk {

RenderPass::~RenderPass() { }

bool RenderPass::init(StringView name, gl::RenderPassType type, gl::RenderOrdering ordering, size_t subpassCount) {
	if (gl::RenderPass::init(name, type, ordering, subpassCount)) {
		switch (type) {
		case gl::RenderPassType::Graphics:
		case gl::RenderPassType::Generic:
			_queueOps = QueueOperations::Graphics;
			break;
		case gl::RenderPassType::Compute:
			_queueOps = QueueOperations::Compute;
			break;
		case gl::RenderPassType::Transfer:
			_queueOps = QueueOperations::Transfer;
			break;
		}
		return true;
	}
	return false;
}

void RenderPass::invalidate() { }

Rc<gl::RenderPassHandle> RenderPass::makeFrameHandle(const gl::FrameQueue &handle) {
	return Rc<RenderPassHandle>::create(*this, handle);
}

RenderPassHandle::~RenderPassHandle() {
	invalidate();
}

void RenderPassHandle::invalidate() {
	if (_pool) {
		_device->releaseCommandPoolUnsafe(move(_pool));
		_pool = nullptr;
	}

	if (_fence) {
		_device->releaseFence(move(_fence));
		_fence = nullptr;
	}

	if (_queue) {
		_device->releaseQueue(move(_queue));
		_queue = nullptr;
	}

	_sync.waitAttachment.clear();
	_sync.waitSem.clear();
	_sync.waitStages.clear();
	_sync.signalSem.clear();
	_sync.signalAttachment.clear();
}

bool RenderPassHandle::prepare(gl::FrameQueue &q, Function<void(bool)> &&cb) {
	_onPrepared = move(cb);
	_device = (Device *)q.getFrame().getDevice();
	_pool = _device->acquireCommandPool(getQueueOps());
	if (!_pool) {
		invalidate();
		return false;
	}

	_sync = makeSyncInfo();

	// If updateAfterBind feature supported for all renderpass bindings
	// - we can use separate thread to update them
	// (ordering of bind|update is not defined in this case)

	if (_data->hasUpdateAfterBind) {
		q.getFrame().performInQueue([this] (gl::FrameHandle &frame) {
			return ((RenderPassImpl *)_data->impl.get())->writeDescriptors(*this, true);
		}, [this] (gl::FrameHandle &frame, bool success) {
			if (!success) {
				_valid = false;
				log::vtext("VK-Error", "Fail to doPrepareDescriptors");
			}

			_descriptorsReady = true;
			if (_commandsReady && _descriptorsReady) {
				_onPrepared(_valid);
				_onPrepared = nullptr;
			}
		}, this, "RenderPass::doPrepareDescriptors");
	} else {
		_descriptorsReady = true;
	}

	q.getFrame().performInQueue([this] (gl::FrameHandle &frame) {
		if (!((RenderPassImpl *)_data->impl.get())->writeDescriptors(*this, false)) {
			return false;
		}

		auto ret = doPrepareCommands(frame);
		if (!ret.empty()) {
			_buffers = move(ret);
			return true;
		}
		return false;
	}, [this, cb] (gl::FrameHandle &frame, bool success) {
		if (!success) {
			log::vtext("VK-Error", "Fail to doPrepareCommands");
			_valid = false;
		}

		_commandsReady = true;
		if (_commandsReady && _descriptorsReady) {
			_onPrepared(_valid);
			_onPrepared = nullptr;
		}
	}, this, "RenderPass::doPrepareCommands");
	return false;
}

void RenderPassHandle::submit(gl::FrameQueue &q, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	Rc<gl::FrameHandle> f = &q.getFrame(); // capture frame ref

	_fence = _device->acquireFence(q.getFrame().getOrder());
	_fence->addRelease([dev = _device, pool = move(_pool), loop = q.getLoop()] (bool success) {
		dev->releaseCommandPool(*loop, Rc<CommandPool>(pool));
	}, nullptr, "RenderPassHandle::submit dev->releaseCommandPool");
	_fence->addRelease([func = move(onComplete)] (bool success) {
		func(success);
	}, nullptr, "RenderPassHandle::submit onComplete");

	_pool = nullptr;

	auto ops = getQueueOps();

	_device->acquireQueue(ops, *f.get(), [this, onSubmited = move(onSubmited)]  (gl::FrameHandle &frame, const Rc<DeviceQueue> &queue) mutable {
		_queue = queue;

		frame.performInQueue([this] (gl::FrameHandle &frame) {
			if (!doSubmit()) {
				return false;
			}
			return true;
		}, [this, onSubmited = move(onSubmited)] (gl::FrameHandle &frame, bool success) {
			if (_queue) {
				_device->releaseQueue(move(_queue));
				_queue = nullptr;
			}
			if (success) {
				onSubmited(true);
				_device->scheduleFence(*frame.getLoop(), move(_fence));
				_fence = nullptr;
				invalidate();
			} else {
				log::vtext("VK-Error", "Fail to vkQueueSubmit");
				_device->releaseFence(move(_fence));
				_fence = nullptr;
				onSubmited(false);
				invalidate();
			}
		}, this, "RenderPass::submit");
	}, [this] (gl::FrameHandle &frame) {
		invalidate();
	}, this);
}

void RenderPassHandle::finalize(gl::FrameQueue &, bool success) {
	/*if (!_commandsReady && !success && _swapchain) {
		for (auto &it : _sync.swapchainSync) {
			_swapchain->releaseSwapchainSync(Rc<SwapchainSync>(it));
		}
	}*/
}

QueueOperations RenderPassHandle::getQueueOps() const {
	return ((RenderPass *)_renderPass.get())->getQueueOps();
}

Vector<VkCommandBuffer> RenderPassHandle::doPrepareCommands(gl::FrameHandle &) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);

	auto currentExtent = getFramebuffer()->getExtent();

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return Vector<VkCommandBuffer>();
	}

	_data->impl.cast<RenderPassImpl>()->perform(*this, buf, [&] {
		VkViewport viewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
		table->vkCmdSetViewport(buf, 0, 1, &viewport);

		VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
		table->vkCmdSetScissor(buf, 0, 1, &scissorRect);

		auto pipeline = _data->subpasses[0].pipelines.get(StringView("Default"));

		table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((Pipeline *)pipeline->pipeline.get())->getPipeline());
		table->vkCmdDraw(buf, 3, 1, 0, 0);
	});

	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

bool RenderPassHandle::doSubmit() {
	auto table = _device->getTable();
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = _sync.waitSem.size();
	submitInfo.pWaitSemaphores = _sync.waitSem.data();
	submitInfo.pWaitDstStageMask = _sync.waitStages.data();
	submitInfo.commandBufferCount = _buffers.size();
	submitInfo.pCommandBuffers = _buffers.data();
	submitInfo.signalSemaphoreCount = _sync.signalSem.size();
	submitInfo.pSignalSemaphores = _sync.signalSem.data();

	if (table->vkQueueSubmit(_queue->getQueue(), 1, &submitInfo, _fence->getFence()) == VK_SUCCESS) {
		// mark semaphores

		/*for (auto &it : _sync.waitSwapchainSync) {
			it->getImageReady()->setSignaled(false);
		}
		for (auto &it : _sync.signalSwapchainSync) {
			it->getRenderFinished()->setSignaled(true);
		}*/

		return true;
	}
	return false;
}

RenderPassHandle::MaterialBuffers RenderPassHandle::updateMaterials(gl::FrameHandle &iframe, const Rc<gl::MaterialSet> &data,
		const Vector<Rc<gl::Material>> &materials, SpanView<gl::MaterialId> dynamicMaterials, SpanView<gl::MaterialId> materialsToRemove) {
	MaterialBuffers ret;
	auto &layout = _device->getTextureSetLayout();

	// update list of materials in set
	data->updateMaterials(materials, dynamicMaterials, materialsToRemove, [&] (const gl::MaterialImage &image) -> Rc<gl::ImageView> {
		return Rc<ImageView>::create(*_device, (Image *)image.image->image.get(), image.info);
	});

	for (auto &it : data->getLayouts()) {
		iframe.performRequiredTask([layout, data, target = &it] (gl::FrameHandle &handle) {
			auto dev = (Device *)handle.getDevice();

			target->set = Rc<gl::TextureSet>(layout->acquireSet(*dev));
			target->set->write(*target);
		}, this, "RenderPassHandle::updateMaterials");
	}

	auto &bufferInfo = data->getInfo();

	auto &frame = static_cast<FrameHandle &>(iframe);
	auto &pool = frame.getMemPool();

	ret.stagingBuffer = pool->spawn(AllocationUsage::HostTransitionSource,
			gl::BufferInfo(gl::ForceBufferUsage(gl::BufferUsage::TransferSrc), bufferInfo.size));
	ret.targetBuffer = pool->spawnPersistent(AllocationUsage::DeviceLocal, bufferInfo);

	auto mapped = ret.stagingBuffer->map();

	uint32_t idx = 0;
	ret.ordering.reserve(data->getMaterials().size());

	uint8_t *target = mapped.ptr;
	for (auto &it : data->getMaterials()) {
		data->encode(target, it.second.get());
 		target += data->getObjectSize();
 		ret.ordering.emplace(it.first, idx);
 		++ idx;
	}

	ret.stagingBuffer->unmap(mapped);
	return ret;
}

RenderPassHandle::Sync RenderPassHandle::makeSyncInfo() {
	Sync sync;
	return sync;
}


VertexRenderPass::~VertexRenderPass() { }

bool VertexRenderPass::init(StringView name, gl::RenderOrdering ordering, size_t subpassCount) {
	return RenderPass::init(name, gl::RenderPassType::Graphics, ordering, subpassCount);
}

Rc<gl::RenderPassHandle> VertexRenderPass::makeFrameHandle(const gl::FrameQueue &handle) {
	return Rc<VertexRenderPassHandle>::create(*this, handle);
}

void VertexRenderPass::prepare(gl::Device &dev) {
	RenderPass::prepare(dev);
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<VertexBufferAttachment *>(it->getAttachment())) {
			_vertexes = a;
		}
	}
}

VertexRenderPassHandle::~VertexRenderPassHandle() { }

bool VertexRenderPassHandle::prepare(gl::FrameQueue &queue, Function<void(bool)> &&cb) {
	if (auto vertexes = queue.getAttachment(((VertexRenderPass *)_renderPass.get())->getVertexes())) {
		_mainBuffer = (VertexBufferAttachmentHandle *)vertexes->handle.get();
	}
	return prepare(queue, move(cb));
}

Vector<VkCommandBuffer> VertexRenderPassHandle::doPrepareCommands(gl::FrameHandle &) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);
	auto pass = (RenderPassImpl *)_data->impl.get();
	auto currentExtent = getFramebuffer()->getExtent();

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return Vector<VkCommandBuffer>();
	}

	auto fb = (Framebuffer *)getFramebuffer().get();

	VkRenderPassBeginInfo renderPassInfo { };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = _data->impl.cast<RenderPassImpl>()->getRenderPass();
	renderPassInfo.framebuffer = fb->getFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VkExtent2D{currentExtent.width, currentExtent.height};
	VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;
	table->vkCmdBeginRenderPass(buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{ 0.0f, 0.0f, float(currentExtent.width), float(currentExtent.height), 0.0f, 1.0f };
	table->vkCmdSetViewport(buf, 0, 1, &viewport);

	VkRect2D scissorRect{ { 0, 0}, { currentExtent.width, currentExtent.height } };
	table->vkCmdSetScissor(buf, 0, 1, &scissorRect);

	auto pipeline = _data->subpasses[0].pipelines.get(StringView("Vertexes"));
	table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((Pipeline *)pipeline->pipeline.get())->getPipeline());

	auto idx = _mainBuffer->getIndexes()->getBuffer();

	table->vkCmdBindIndexBuffer(buf, idx, 0, VK_INDEX_TYPE_UINT32);

	table->vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->getPipelineLayout(), 0,
			pass->getDescriptorSets().size(), pass->getDescriptorSets().data(), 0, nullptr);
	table->vkCmdDrawIndexed(buf, 6, 1, 0, 0, 0);

	table->vkCmdEndRenderPass(buf);
	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

bool VertexRenderPassHandle::doSubmit() {
	return RenderPassHandle::doSubmit();
}

}
