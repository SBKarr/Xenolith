/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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
#include "XLVkRenderPassImpl.h"
#include "XLVkImageAttachment.h"
#include "XLVkDevice.h"
#include "XLGlFrame.h"
#include "XLGlLoop.h"
#include <forward_list>

namespace stappler::xenolith::vk {

RenderPass::~RenderPass() { }

void RenderPass::invalidate() { }

Rc<gl::RenderPassHandle> RenderPass::makeFrameHandle(gl::RenderPassData *data, const gl::FrameHandle &handle) {
	return Rc<RenderPassHandle>::create(*this, data, handle);
}

RenderPassHandle::~RenderPassHandle() {
	invalidate();
}

void RenderPassHandle::invalidate() {
	if (_pool) {
		_device->releaseCommandPool(move(_pool));
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

bool RenderPassHandle::prepare(gl::FrameHandle &frame) {
	_device = (Device *)frame.getDevice();
	_pool = _device->acquireCommandPool(QueueOperations::Graphics);
	if (!_pool) {
		invalidate();
		return false;
	}

	uint32_t index = 0;
	for (auto &it : _attachments) {
		if (it.first->getType() == gl::AttachmentType::SwapchainImage) {
			auto img = it.second.cast<SwapchainAttachmentHandle>();
			index = img->getIndex();
		}
	}

	if (index == maxOf<uint32_t>()) {
		invalidate();
		return false;
	}

	frame.performInQueue([this, index] (gl::FrameHandle &frame) {
		return doPrepareDescriptors(frame, index);
	}, [this] (gl::FrameHandle &frame, bool success) {
		if (success) {
			_descriptorsReady = true;
			if (_commandsReady && _descriptorsReady) {
				frame.setRenderPassPrepared(this);
			}
		} else {
			log::vtext("VK-Error", "Fail to doPrepareDescriptors");
			frame.invalidate();
		}
	}, this);

	frame.performInQueue([this, index] (gl::FrameHandle &frame) {
		/*if (!doPrepareDescriptors(frame, index)) {
			return false;
		}*/

		auto ret = doPrepareCommands(frame, index);
		if (!ret.empty()) {
			_buffers = move(ret);
			return true;
		}
		return false;
	}, [this] (gl::FrameHandle &frame, bool success) {
		if (success) {
			_commandsReady = true;
			//_descriptorsReady = true;
			if (_commandsReady && _descriptorsReady) {
				frame.setRenderPassPrepared(this);
			}
		} else {
			log::vtext("VK-Error", "Fail to doPrepareCommands");
			frame.invalidate();
		}
	}, this);
	return false;
}

void RenderPassHandle::submit(gl::FrameHandle &frame, Function<void(const Rc<gl::RenderPass> &)> &&func) {
	Rc<gl::FrameHandle> f = &frame; // capture frame ref

	_fence = _device->acquireFence(frame.getOrder());
	_fence->addRelease([dev = _device, pool = move(_pool)] {
		dev->releaseCommandPool(Rc<CommandPool>(pool));
	});
	_fence->addRelease([func = move(func), pass = _renderPass] {
		func(pass);
	});

	_pool = nullptr;
	_sync = makeSyncInfo();
	for (auto &it : _sync.swapchainSync) {
		_fence->addRelease([dev = _device, sync = it] {
			dev->releaseSwapchainSync(Rc<SwapchainSync>(sync));
		});
	}

	_device->acquireQueue(QueueOperations::Graphics, frame, [this] (gl::FrameHandle &frame, const Rc<DeviceQueue> &queue) {
		_queue = queue;
		if (_data->isPresentable) {
			for (auto &it : _attachments) {
				if (it.first->getType() == gl::AttachmentType::SwapchainImage) {
					if (auto d = it.second.cast<SwapchainAttachmentHandle>()) {
						_presentAttachment = d;
					}
				}
			}
		}

		frame.performInQueue([this] (gl::FrameHandle &frame) {
			auto table = _device->getTable();
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			submitInfo.waitSemaphoreCount = _sync.waitSem.size();
			submitInfo.pWaitSemaphores = _sync.waitSem.data();
			submitInfo.pWaitDstStageMask = _sync.waitStages.data();
			submitInfo.commandBufferCount = _buffers.size();
			submitInfo.pCommandBuffers = _buffers.data();
			submitInfo.signalSemaphoreCount = _sync.signalSem.size();
			submitInfo.pSignalSemaphores = _sync.signalSem.data();

			if (table->vkQueueSubmit(_queue->getQueue(), 1, &submitInfo, _fence->getFence()) != VK_SUCCESS) {
				return false;
			}

			if (_presentAttachment) {
				if ((_queue->getOps() & QueueOperations::Present) != QueueOperations::None) {
					present(frame);
				} else {
					// TODO - present on another queue
				}
			}

			return true;
		}, [this] (gl::FrameHandle &frame, bool success) {
			if (success) {
				if (_queue) {
					_device->releaseQueue(move(_queue));
					_queue = nullptr;
				}
				_device->scheduleFence(*frame.getLoop(), move(_fence));
				_fence = nullptr;
				frame.setRenderPassSubmitted(this);
				invalidate();
			} else {
				if (_queue) {
					_device->releaseQueue(move(_queue));
					_queue = nullptr;
				}
				log::vtext("VK-Error", "Fail to vkQueueSubmit");
				_device->releaseFence(move(_fence));
				_fence = nullptr;
				invalidate();
				frame.invalidate();
			}
		}, this);
	}, [this] (gl::FrameHandle &frame) {
		invalidate();
	}, this);
}

bool RenderPassHandle::doPrepareDescriptors(gl::FrameHandle &frame, uint32_t index) {
	auto table = _device->getTable();
	auto pass = (RenderPassImpl *)_data->impl.get();

	std::forward_list<Vector<VkDescriptorImageInfo>> images;
	std::forward_list<Vector<VkDescriptorBufferInfo>> buffers;
	std::forward_list<Vector<VkBufferView>> views;

	Vector<VkWriteDescriptorSet> writes;

	auto writeDescriptor = [&] (VkDescriptorSet set, const gl::PipelineDescriptor &desc, uint32_t currentDescriptor, bool external) {
		auto a = getAttachmentHandle(desc.attachment);
		if (!a) {
			return false;
		}

		Vector<VkDescriptorImageInfo> *localImages = nullptr;
		Vector<VkDescriptorBufferInfo> *localBuffers = nullptr;
		Vector<VkBufferView> *localViews = nullptr;

		VkWriteDescriptorSet writeData;
		writeData.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeData.pNext = nullptr;
		writeData.dstSet = set;
		writeData.dstBinding = currentDescriptor;
		writeData.dstArrayElement = 0;
		writeData.descriptorCount = 0;
		writeData.descriptorType = VkDescriptorType(desc.type);
		writeData.pImageInfo = VK_NULL_HANDLE;
		writeData.pBufferInfo = VK_NULL_HANDLE;
		writeData.pTexelBufferView = VK_NULL_HANDLE;

		auto c = a->getDescriptorArraySize(*this, desc, external);
		for (uint32_t i = 0; i < c; ++ i) {
			if (a->isDescriptorDirty(*this, desc, i, external)) {
				switch (desc.type) {
				case gl::DescriptorType::Sampler:
				case gl::DescriptorType::CombinedImageSampler:
				case gl::DescriptorType::SampledImage:
				case gl::DescriptorType::StorageImage:
				case gl::DescriptorType::InputAttachment:
					if (!localImages) {
						localImages = &images.emplace_front(Vector<VkDescriptorImageInfo>());
					}
					if (localImages) {
						auto &dst = localImages->emplace_back(VkDescriptorImageInfo());
						auto h = (ImageAttachmentHandle *)a;
						if (!h->writeDescriptor(*this, desc, i, external, dst)) {
							return false;
						}
					}
					break;
				case gl::DescriptorType::StorageTexelBuffer:
				case gl::DescriptorType::UniformTexelBuffer:
					if (!localViews) {
						localViews = &views.emplace_front(Vector<VkBufferView>());
					}
					if (localViews) {
						auto h = (TexelAttachmentHandle *)a;
						if (auto v = h->getDescriptor(*this, desc, i, external)) {
							localViews->emplace_back(v);
						} else {
							return false;
						}
					}
					break;
				case gl::DescriptorType::UniformBuffer:
				case gl::DescriptorType::StorageBuffer:
				case gl::DescriptorType::UniformBufferDynamic:
				case gl::DescriptorType::StorageBufferDynamic:
					if (!localBuffers) {
						localBuffers = &buffers.emplace_front(Vector<VkDescriptorBufferInfo>());
					}
					if (localBuffers) {
						auto &dst = localBuffers->emplace_back(VkDescriptorBufferInfo());
						auto h = (BufferAttachmentHandle *)a;
						if (!h->writeDescriptor(*this, desc, i, external, dst)) {
							return false;
						}
					}
					break;
				case gl::DescriptorType::Unknown:
					break;
				}
				++ writeData.descriptorCount;
			} else {
				if (writeData.descriptorCount > 0) {
					if (localImages) {
						writeData.pImageInfo = localImages->data();
					}
					if (localBuffers) {
						writeData.pBufferInfo = localBuffers->data();
					}
					if (localViews) {
						writeData.pTexelBufferView = localViews->data();
					}

					writes.emplace_back(move(writeData));

					writeData.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeData.pNext = nullptr;
					writeData.dstSet = set;
					writeData.dstBinding = currentDescriptor;
					writeData.descriptorCount = 0;
					writeData.descriptorType = VkDescriptorType(desc.type);
					writeData.pImageInfo = VK_NULL_HANDLE;
					writeData.pBufferInfo = VK_NULL_HANDLE;
					writeData.pTexelBufferView = VK_NULL_HANDLE;

					localImages = nullptr;
					localBuffers = nullptr;
					localViews = nullptr;
				}

				writeData.dstArrayElement = i + 1;
			}
		}

		if (writeData.descriptorCount > 0) {
			if (localImages) {
				writeData.pImageInfo = localImages->data();
			}
			if (localBuffers) {
				writeData.pBufferInfo = localBuffers->data();
			}
			if (localViews) {
				writeData.pTexelBufferView = localViews->data();
			}

			writes.emplace_back(move(writeData));
		}
		return true;
	};

	uint32_t currentSet = 0;
	if (!_data->pipelineLayout.queueDescriptors.empty()) {
		auto set = pass->getDescriptorSet(currentSet);
		uint32_t currentDescriptor = 0;
		for (auto &it : _data->pipelineLayout.queueDescriptors) {
			if (!writeDescriptor(set, *it, currentDescriptor, false)) {
				return false;
			}
			++ currentDescriptor;
		}
		++ currentSet;
	}

	if (!_data->pipelineLayout.extraDescriptors.empty()) {
		auto set = pass->getDescriptorSet(currentSet);
		uint32_t currentDescriptor = 0;
		for (auto &it : _data->pipelineLayout.extraDescriptors) {
			if (!writeDescriptor(set, it, currentDescriptor, true)) {
				return false;
			}
			++ currentDescriptor;
		}
		++ currentSet;
	}

	if (writes.empty()) {
		return true;
	}

	table->vkUpdateDescriptorSets(_device->getDevice(), writes.size(), writes.data(), 0, nullptr);
	return true;
}

Vector<VkCommandBuffer> RenderPassHandle::doPrepareCommands(gl::FrameHandle &frame, uint32_t index) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);

	auto targetFb = _data->framebuffers[index].cast<Framebuffer>();
	auto currentExtent = targetFb->getExtent();

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return Vector<VkCommandBuffer>();
	}

	VkRenderPassBeginInfo renderPassInfo { };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = _data->impl.cast<RenderPassImpl>()->getRenderPass();
	renderPassInfo.framebuffer = targetFb->getFramebuffer();
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

	auto pipeline = _data->pipelines.get(StringView("Default"));

	table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((Pipeline *)pipeline->pipeline.get())->getPipeline());
	table->vkCmdDraw(buf, 3, 1, 0, 0);
	table->vkCmdEndRenderPass(buf);
	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

bool RenderPassHandle::present(gl::FrameHandle &frame) {
	auto table = _device->getTable();

	Vector<VkSemaphore> presentSem;

	auto imageIndex = _presentAttachment->getIndex();
	for (auto &it : _sync.swapchainSync) {
		presentSem.emplace_back(it->getRenderFinished()->getSemaphore());
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = presentSem.size();
	presentInfo.pWaitSemaphores = presentSem.data();

	VkSwapchainKHR swapChains[] = {_presentAttachment->getSwapchain()->getSwapchain()};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	// auto t = platform::device::_clock();
	auto result = table->vkQueuePresentKHR(_queue->getQueue(), &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		frame.performOnGlThread([this] (gl::FrameHandle &frame) {
			frame.getLoop()->pushContextEvent(gl::Loop::Event::SwapChainDeprecated);
			frame.invalidate();
		});
	} else if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
		return true;
	} else {
		log::vtext("VK-Error", "Fail to vkQueuePresentKHR: ", result);
		frame.performOnGlThread([this] (gl::FrameHandle &frame) {
			frame.invalidate();
		});
	}

	return false;
}

RenderPassHandle::Sync RenderPassHandle::makeSyncInfo() {
	Sync sync;
	for (auto &it : _attachments) {
		if (it.first->getType() == gl::AttachmentType::SwapchainImage) {
			if (auto d = it.second.cast<SwapchainAttachmentHandle>()) {
				if (it.first->getFirstRenderPass() == _data) {
					sync.waitAttachment.emplace_back(it.second);
					sync.waitSem.emplace_back(d->getSync()->getImageReady()->getSemaphore());
					sync.waitStages.emplace_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
					d->getSync()->getImageReady()->setSignaled(false);
				}

				if (it.first->getLastRenderPass() == _data) {
					sync.signalSem.emplace_back(d->getSync()->getRenderFinished()->getUnsignalled());
					sync.signalAttachment.emplace_back(it.second);
					d->getSync()->getRenderFinished()->setSignaled(true);
					sync.swapchainSync.emplace_back(d->acquireSync());
				}
			}
		}
	}

	return sync;
}


VertexRenderPass::~VertexRenderPass() {

}

Rc<gl::RenderPassHandle> VertexRenderPass::makeFrameHandle(gl::RenderPassData *data, const gl::FrameHandle &handle) {
	return Rc<VertexRenderPassHandle>::create(*this, data, handle);
}

VertexRenderPassHandle::~VertexRenderPassHandle() { }

void VertexRenderPassHandle::addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) {
	RenderPassHandle::addRequiredAttachment(a, h);
	if (h->isInput()) {
		if (auto b = dynamic_cast<VertexBufferAttachmentHandle *>(h.get())) {
			_mainBuffer = b;
		}
	}
}

Vector<VkCommandBuffer> VertexRenderPassHandle::doPrepareCommands(gl::FrameHandle &, uint32_t index) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);
	auto pass = (RenderPassImpl *)_data->impl.get();

	auto targetFb = _data->framebuffers[index].cast<Framebuffer>();
	auto currentExtent = targetFb->getExtent();

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return Vector<VkCommandBuffer>();
	}

	Vector<VkBufferMemoryBarrier> bufferBarriers;
	bufferBarriers.emplace_back(VkBufferMemoryBarrier({
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		_mainBuffer->getVertexes()->getBuffer(), 0, VK_WHOLE_SIZE
	}));

	bufferBarriers.emplace_back(VkBufferMemoryBarrier({
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDEX_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		_mainBuffer->getIndexes()->getBuffer(), 0, VK_WHOLE_SIZE
	}));

	table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0,
			0, nullptr,
			bufferBarriers.size(), bufferBarriers.data(),
			0, nullptr);

	VkRenderPassBeginInfo renderPassInfo { };
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = _data->impl.cast<RenderPassImpl>()->getRenderPass();
	renderPassInfo.framebuffer = targetFb->getFramebuffer();
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

	table->vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->getPipelineLayout(), 0,
			pass->getDescriptorSets().size(), pass->getDescriptorSets().data(), 0, nullptr);

	auto pipeline = _data->pipelines.get(StringView("Vertexes"));
	table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((Pipeline *)pipeline->pipeline.get())->getPipeline());

	auto idx = _mainBuffer->getIndexes()->getBuffer();

	table->vkCmdBindIndexBuffer(buf, idx, 0, VK_INDEX_TYPE_UINT32);

	table->vkCmdDrawIndexed(buf, 6, 1, 0, 0, 0);
	//table->vkCmdDraw(buf, 1, 1, 0, 0);

	table->vkCmdEndRenderPass(buf);
	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

}
