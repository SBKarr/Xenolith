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

#include "XLVkQueuePass.h"

#include "XLRenderQueueFrameHandle.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkAttachment.h"
#include "XLVkDevice.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::vk {

QueuePass::~QueuePass() { }

bool QueuePass::init(StringView name, PassType type, RenderOrdering ordering, size_t subpassCount) {
	if (renderqueue::Pass::init(name, type, ordering, subpassCount)) {
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

void QueuePass::invalidate() { }

auto QueuePass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<QueuePassHandle>::create(*this, handle);
}

QueuePassHandle::~QueuePassHandle() {
	invalidate();
}

void QueuePassHandle::invalidate() {
	if (_pool) {
		_device->releaseCommandPoolUnsafe(move(_pool));
		_pool = nullptr;
	}

	if (_queue) {
		_device->releaseQueue(move(_queue));
		_queue = nullptr;
	}

	_sync = nullptr;
}

bool QueuePassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	_onPrepared = move(cb);
	_loop = (Loop *)q.getLoop();
	_device = (Device *)q.getFrame()->getDevice();
	_pool = _device->acquireCommandPool(getQueueOps());
	if (!_pool) {
		invalidate();
		return false;
	}

	// If updateAfterBind feature supported for all renderpass bindings
	// - we can use separate thread to update them
	// (ordering of bind|update is not defined in this case)

	if (_data->hasUpdateAfterBind) {
		q.getFrame()->performInQueue([this] (FrameHandle &frame) {
			return ((RenderPassImpl *)_data->impl.get())->writeDescriptors(*this, true);
		}, [this] (FrameHandle &frame, bool success) {
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

	q.getFrame()->performInQueue([this] (FrameHandle &frame) {
		if (!((RenderPassImpl *)_data->impl.get())->writeDescriptors(*this, false)) {
			return false;
		}

		auto ret = doPrepareCommands(frame);
		if (!ret.empty()) {
			_buffers = move(ret);
			return true;
		}
		return false;
	}, [this, cb] (FrameHandle &frame, bool success) {
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

void QueuePassHandle::submit(FrameQueue &q, Rc<FrameSync> &&sync, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	Rc<FrameHandle> f = q.getFrame(); // capture frame ref

	_fence = _loop->acquireFence(f->getOrder());

	_fence->setTag(getName());

	_fence->addRelease([dev = _device, pool = _pool, loop = q.getLoop()] (bool success) {
		dev->releaseCommandPool(*loop, Rc<CommandPool>(pool));
	}, nullptr, "RenderPassHandle::submit dev->releaseCommandPool");
	_fence->addRelease([this, func = move(onComplete), q = &q] (bool success) mutable {
		doComplete(*q, move(func), success);
	}, this, "RenderPassHandle::submit onComplete");

	_sync = move(sync);

	auto ops = getQueueOps();

	_device->acquireQueue(ops, *f.get(), [this, onSubmited = move(onSubmited)]  (FrameHandle &frame, const Rc<DeviceQueue> &queue) mutable {
		_queue = queue;

		frame.performInQueue([this, onSubmited = move(onSubmited)] (FrameHandle &frame) mutable {
			if (!doSubmit(frame, move(onSubmited))) {
				return false;
			}
			return true;
		}, this, "RenderPass::submit");
	}, [this] (FrameHandle &frame) {
		_sync = nullptr;
		invalidate();
	}, this);
}

void QueuePassHandle::finalize(FrameQueue &, bool success) {

}

QueueOperations QueuePassHandle::getQueueOps() const {
	return ((QueuePass *)_renderPass.get())->getQueueOps();
}

Vector<VkCommandBuffer> QueuePassHandle::doPrepareCommands(FrameHandle &) {
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

		auto pipeline = _data->subpasses[0].graphicPipelines.get(StringView("Default"));

		table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((GraphicPipeline *)pipeline->pipeline.get())->getPipeline());
		table->vkCmdDraw(buf, 3, 1, 0, 0);
	});

	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

bool QueuePassHandle::doSubmit(FrameHandle &frame, Function<void(bool)> &&onSubmited) {
	auto success = _queue->submit(*_sync, *_fence, *_pool, _buffers);
	_pool = nullptr;
	frame.performOnGlThread([this, success, onSubmited = move(onSubmited), queue = move(_queue)] (FrameHandle &frame) mutable {
		bool scheduleWithSwapcchain = false;

		for (auto &it : _sync->images) {
			if (it.image->isSwapchainImage()) {
				scheduleWithSwapcchain = true;
			}
		}

		if (queue) {
			_device->releaseQueue(move(queue));
			queue = nullptr;
		}
		if (success) {
			if (scheduleWithSwapcchain) {
				// from frame's perspective, onComplete event, binded with fence,
				// will occur on next Loop clock, after onSubmit event
				// but in View's perspective fence will be scheduled before presentation to allow wait on it
				// so, fence scheduling and submission event is inverted in this case
				if (auto &view = frame.getSwapchain()) {
					((View *)view.get())->scheduleFence(move(_fence));
				}
				onSubmited(true);
			} else {
				onSubmited(true);
				_fence->schedule(*_loop);
			}
			_fence = nullptr;
			invalidate();
		} else {
			log::vtext("VK-Error", "Fail to vkQueueSubmit");
			_fence->schedule(*_loop);
			_fence = nullptr;
			onSubmited(false);
			invalidate();
		}
		_sync = nullptr;
	}, nullptr, false, "RenderPassHandle::doSubmit");
	return success;
}

void QueuePassHandle::doComplete(FrameQueue &, Function<void(bool)> &&func, bool success) {
	func(success);
}

auto QueuePassHandle::updateMaterials(FrameHandle &frame, const Rc<gl::MaterialSet> &data, const Vector<Rc<gl::Material>> &materials,
		SpanView<gl::MaterialId> dynamicMaterials, SpanView<gl::MaterialId> materialsToRemove) -> MaterialBuffers {
	MaterialBuffers ret;
	auto &layout = _device->getTextureSetLayout();

	// update list of materials in set
	auto updated = data->updateMaterials(materials, dynamicMaterials, materialsToRemove, [&] (const gl::MaterialImage &image) -> Rc<gl::ImageView> {
		return Rc<ImageView>::create(*_device, (Image *)image.image->image.get(), image.info);
	});
	if (updated.empty()) {
		return MaterialBuffers();
	}

	for (auto &it : data->getLayouts()) {
		frame.performRequiredTask([layout, data, target = &it] (FrameHandle &handle) {
			auto dev = (Device *)handle.getDevice();

			target->set = Rc<gl::TextureSet>(layout->acquireSet(*dev));
			target->set->write(*target);
			return true;
		}, this, "RenderPassHandle::updateMaterials");
	}

	auto &bufferInfo = data->getInfo();

	auto pool = static_cast<DeviceFrameHandle &>(frame).getMemPool(&frame);

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


VertexPass::~VertexPass() { }

bool VertexPass::init(StringView name, RenderOrdering ordering, size_t subpassCount) {
	return QueuePass::init(name, gl::RenderPassType::Graphics, ordering, subpassCount);
}

auto VertexPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<VertexPassHandle>::create(*this, handle);
}

void VertexPass::prepare(gl::Device &dev) {
	QueuePass::prepare(dev);
	for (auto &it : _data->descriptors) {
		if (auto a = dynamic_cast<VertexBufferAttachment *>(it->getAttachment())) {
			_vertexes = a;
		}
	}
}

VertexPassHandle::~VertexPassHandle() { }

bool VertexPassHandle::prepare(FrameQueue &queue, Function<void(bool)> &&cb) {
	if (auto vertexes = queue.getAttachment(((VertexPass *)_renderPass.get())->getVertexes())) {
		_mainBuffer = (VertexBufferAttachmentHandle *)vertexes->handle.get();
	}
	return QueuePassHandle::prepare(queue, move(cb));
}

Vector<VkCommandBuffer> VertexPassHandle::doPrepareCommands(FrameHandle &) {
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

	auto pipeline = _data->subpasses[0].graphicPipelines.get(StringView("Vertexes"));
	table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((GraphicPipeline *)pipeline->pipeline.get())->getPipeline());

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

bool VertexPassHandle::doSubmit(FrameHandle &frame, Function<void(bool)> &&onSubmited) {
	return QueuePassHandle::doSubmit(frame, move(onSubmited));
}

}
