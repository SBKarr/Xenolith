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
#include "XLVkRenderPassImpl.h"
#include "XLVkImageAttachment.h"
#include "XLVkDevice.h"
#include "XLGlFrame.h"
#include "XLGlLoop.h"
#include <forward_list>

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
	_swapchain = (Swapchain *)frame.getSwapchain();
	_pool = _device->acquireCommandPool(getQueueOps());
	if (!_pool) {
		invalidate();
		return false;
	}

	if (_data->isPresentable) {
		for (auto &it : _attachments) {
			if (it.first->getType() == gl::AttachmentType::SwapchainImage) {
				if (auto d = it.second.cast<SwapchainAttachmentHandle>()) {
					_presentAttachment = d;
				}
			}
		}
	}

	_sync = makeSyncInfo();
	for (auto &it : _sync.swapchainSync) {
		if (it->getImageIndex() == maxOf<uint32_t>()) {
			return false;
		}
	}

	// If updateAfterBind feature supported for all renderpass bindings
	// - we can use separate thread to update them
	// (ordering of bind|update is not defined in this case)

	if (_data->hasUpdateAfterBind) {
		frame.performInQueue([this] (gl::FrameHandle &frame) {
			return doPrepareDescriptors(frame, true);
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
		}, this, "RenderPass::doPrepareDescriptors");
	} else {
		_descriptorsReady = true;
	}

	frame.performInQueue([this] (gl::FrameHandle &frame) {
		if (!doPrepareDescriptors(frame, false)) {
			return false;
		}

		auto ret = doPrepareCommands(frame);
		if (!ret.empty()) {
			_buffers = move(ret);
			return true;
		}
		return false;
	}, [this] (gl::FrameHandle &frame, bool success) {
		if (success) {
			_commandsReady = true;
			if (_commandsReady && _descriptorsReady) {
				frame.setRenderPassPrepared(this);
			}
		} else {
			log::vtext("VK-Error", "Fail to doPrepareCommands");
			frame.invalidate();
		}
	}, this, "RenderPass::doPrepareCommands");
	return false;
}

void RenderPassHandle::submit(gl::FrameHandle &frame, Function<void(const Rc<gl::RenderPassHandle> &)> &&func) {
	Rc<gl::FrameHandle> f = &frame; // capture frame ref

	_fence = _device->acquireFence(frame.getOrder());
	_fence->addRelease([dev = _device, pool = move(_pool)] {
		dev->releaseCommandPool(Rc<CommandPool>(pool));
	});
	_fence->addRelease([func = move(func), pass = Rc<gl::RenderPassHandle>(this)] {
		func(pass);
	});

	_pool = nullptr;
	for (auto &it : _sync.swapchainSync) {
		_fence->addRelease([dev = _swapchain, sync = it] {
			dev->releaseSwapchainSync(Rc<SwapchainSync>(sync));
		});
	}

	auto ops = getQueueOps();

	_device->acquireQueue(ops, frame, [this] (gl::FrameHandle &frame, const Rc<DeviceQueue> &queue) {
		_queue = queue;

		frame.performInQueue([this] (gl::FrameHandle &frame) {
			for (auto &it : _sync.swapchainSync) {
				it->lock();
				if (!it->isSwapchainValid()) {
					_isSyncValid = false;
				}
			}

			if (!_isSyncValid || !doSubmit(frame)) {
				for (auto &it : _sync.swapchainSync) {
					it->unlock();
				}
				return false;
			}

			if (_presentAttachment) {
				if ((_queue->getOps() & QueueOperations::Present) != QueueOperations::None) {
					present(frame);
				} else {
					// TODO - present on another queue
				}
			}

			for (auto &it : _sync.swapchainSync) {
				it->unlock();
			}
			return true;
		}, [this] (gl::FrameHandle &frame, bool success) {
			if (_queue) {
				_device->releaseQueue(move(_queue));
				_queue = nullptr;
			}
			if (success) {
				_device->scheduleFence(*frame.getLoop(), move(_fence));
				_fence = nullptr;
				frame.setRenderPassSubmitted(this);
				invalidate();
			} else {
				if (_isSyncValid) {
					log::vtext("VK-Error", "Fail to vkQueueSubmit");
				}
				_device->releaseFence(move(_fence));
				_fence = nullptr;
				invalidate();
				frame.invalidate();
			}
		}, this, "RenderPass::submit");
	}, [this] (gl::FrameHandle &frame) {
		invalidate();
	}, this);
}

void RenderPassHandle::finalize(gl::FrameHandle &, bool success) {
	if (!_commandsReady && !success && _swapchain) {
		for (auto &it : _sync.swapchainSync) {
			_swapchain->releaseSwapchainSync(Rc<SwapchainSync>(it));
		}
	}
}

QueueOperations RenderPassHandle::getQueueOps() const {
	return ((RenderPass *)_renderPass.get())->getQueueOps();
}

bool RenderPassHandle::doPrepareDescriptors(gl::FrameHandle &frame, bool async) {
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
	if (!_data->queueDescriptors.empty()) {
		auto set = pass->getDescriptorSet(currentSet);
		uint32_t currentDescriptor = 0;
		for (auto &it : _data->queueDescriptors) {
			if (it->updateAfterBind != async) {
				continue;
			}
			if (!writeDescriptor(set, *it, currentDescriptor, false)) {
				return false;
			}
			++ currentDescriptor;
		}
		++ currentSet;
	}

	if (!_data->extraDescriptors.empty()) {
		auto set = pass->getDescriptorSet(currentSet);
		uint32_t currentDescriptor = 0;
		for (auto &it : _data->extraDescriptors) {
			if (it.updateAfterBind != async) {
				continue;
			}
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

Vector<VkCommandBuffer> RenderPassHandle::doPrepareCommands(gl::FrameHandle &frame) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);

	auto index = (*_sync.swapchainSync.begin())->getImageIndex();

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

	auto pipeline = _data->subpasses[0].pipelines.get(StringView("Default"));

	table->vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, ((Pipeline *)pipeline->pipeline.get())->getPipeline());
	table->vkCmdDraw(buf, 3, 1, 0, 0);
	table->vkCmdEndRenderPass(buf);
	if (table->vkEndCommandBuffer(buf) == VK_SUCCESS) {
		return Vector<VkCommandBuffer>{buf};
	}
	return Vector<VkCommandBuffer>();
}

bool RenderPassHandle::doSubmit(gl::FrameHandle &) {
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

		for (auto &it : _sync.waitSwapchainSync) {
			it->getImageReady()->setSignaled(false);
		}
		for (auto &it : _sync.signalSwapchainSync) {
			it->getRenderFinished()->setSignaled(true);
		}

		return true;
	}
	return false;
}

bool RenderPassHandle::present(gl::FrameHandle &frame) {
	auto table = _device->getTable();

	Vector<VkSemaphore> presentSem;

	for (auto &it : _sync.signalSwapchainSync) {
		presentSem.emplace_back(it->getRenderFinished()->getSemaphore());
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = presentSem.size();
	presentInfo.pWaitSemaphores = presentSem.data();

	Vector<uint32_t> indexes;
	for (auto &it : _sync.swapchainSync) {
		auto idx = it->getImageIndex();
		if (idx != maxOf<uint32_t>()) {
			indexes.emplace_back(idx);
		} else {
			return false;
		}
	}

	VkSwapchainKHR swapChains[] = {_presentAttachment->getSwapchain()->getSwapchain()};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = indexes.data();
	presentInfo.pResults = nullptr; // Optional

	// auto t = platform::device::_clock();
	auto result = table->vkQueuePresentKHR(_queue->getQueue(), &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		frame.performOnGlThread([this] (gl::FrameHandle &frame) {
			frame.getLoop()->pushContextEvent(gl::Loop::EventName::SwapChainDeprecated, frame.getSwapchain());
			frame.invalidate();
		});
	} else if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
		for (auto &it : _sync.signalSwapchainSync) {
			it->getRenderFinished()->setSignaled(false);
		}
		for (auto &it : _sync.swapchainSync) {
			it->clearImageIndex();
		}
		return true;
	} else {
		log::vtext("VK-Error", "Fail to vkQueuePresentKHR: ", result);
		frame.performOnGlThread([this] (gl::FrameHandle &frame) {
			frame.invalidate();
		});
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
		}, this);
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
	for (auto &it : _attachments) {
		if (it.first->getType() == gl::AttachmentType::SwapchainImage) {
			if (auto d = it.second.cast<SwapchainAttachmentHandle>()) {
				auto firstPass = it.first->getFirstRenderPass();
				auto lastPass = it.first->getLastRenderPass();

				auto dSync = (lastPass == _data) ? d->acquireSync() : d->getSync();

				if (firstPass == _data) {
					// swapchain to wait before submit
					sync.waitAttachment.emplace_back(it.second);
					sync.waitSem.emplace_back(dSync->getImageReady()->getSemaphore());
					sync.waitStages.emplace_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
					sync.swapchainSync.emplace(sync.waitSwapchainSync.emplace_back(dSync));
				}

				if (lastPass == _data) {
					// swapchain to signal after submit
					sync.signalSem.emplace_back(dSync->getRenderFinished()->getUnsignalled());
					sync.signalAttachment.emplace_back(it.second);
					sync.swapchainSync.emplace(sync.signalSwapchainSync.emplace_back(dSync));
				}
			}
		}
	}

	return sync;
}


VertexRenderPass::~VertexRenderPass() { }

bool VertexRenderPass::init(StringView name, gl::RenderOrdering ordering, size_t subpassCount) {
	return RenderPass::init(name, gl::RenderPassType::Graphics, ordering, subpassCount);
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

Vector<VkCommandBuffer> VertexRenderPassHandle::doPrepareCommands(gl::FrameHandle &handle) {
	auto table = _device->getTable();
	auto buf = _pool->allocBuffer(*_device);
	auto pass = (RenderPassImpl *)_data->impl.get();

	auto index = (*_sync.swapchainSync.begin())->getImageIndex();

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

bool VertexRenderPassHandle::doSubmit(gl::FrameHandle &handle) {
	_mainBuffer->writeVertexes(handle);
	return RenderPassHandle::doSubmit(handle);
}

}
