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

#include "XLVkDeviceQueue.h"
#include "XLVkDevice.h"
#include "XLVkSync.h"
#include "XLVkObject.h"
#include "XLRenderQueueImageStorage.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkFramebuffer.h"
#include "XLVkPipeline.h"

namespace stappler::xenolith::vk {

DeviceQueue::~DeviceQueue() { }

bool DeviceQueue::init(Device &device, VkQueue queue, uint32_t index, QueueOperations ops) {
	_device = &device;
	_queue = queue;
	_index = index;
	_ops = ops;
	return true;
}

bool DeviceQueue::submit(const FrameSync &sync, Fence &fence, CommandPool &commandPool, SpanView<const CommandBuffer *> buffers) {
	Vector<VkSemaphore> waitSem;
	Vector<VkPipelineStageFlags> waitStages;
	Vector<VkSemaphore> signalSem;
	Vector<VkCommandBuffer> vkBuffers; vkBuffers.reserve(buffers.size());

	for (auto &it : buffers) {
		if (it) {
			vkBuffers.emplace_back(it->getBuffer());
		}
	}

	for (auto &it : sync.waitAttachments) {
		if (it.semaphore) {
			auto sem = ((Semaphore *)it.semaphore.get())->getSemaphore();

			if (!it.semaphore->isWaited()) {
				waitSem.emplace_back(sem);
				waitStages.emplace_back(VkPipelineStageFlags(it.stages));
			}
		}
	}

	for (auto &it : sync.signalAttachments) {
		if (it.semaphore) {
			auto sem = ((Semaphore *)it.semaphore.get())->getSemaphore();

			signalSem.emplace_back(sem);
		}
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = waitSem.size();
	submitInfo.pWaitSemaphores = waitSem.data();
	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.commandBufferCount = vkBuffers.size();
	submitInfo.pCommandBuffers = vkBuffers.data();
	submitInfo.signalSemaphoreCount = signalSem.size();
	submitInfo.pSignalSemaphores = signalSem.data();

#if XL_VKAPI_DEBUG
	auto t = platform::device::_clock(platform::device::Monotonic);
	fence.addRelease([frameIdx = _frameIdx, t] (bool success) {
		XL_VKAPI_LOG("[", frameIdx,  "] vkQueueSubmit [complete]",
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
	}, nullptr, "DeviceQueue::submit");
#endif

	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = platform::device::_clock(platform::device::Monotonic);
		_result = table.vkQueueSubmit(_queue, 1, &submitInfo, fence.getFence());
		XL_VKAPI_LOG("[", _frameIdx,  "] vkQueueSubmit: ", _result, " ", (void *)_queue,
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
#else
		_result = table.vkQueueSubmit(_queue, 1, &submitInfo, fence.getFence());
#endif
	});

	if (_result == VK_SUCCESS) {
		// mark semaphores

		for (auto &it : sync.waitAttachments) {
			if (it.semaphore) {
				it.semaphore->setWaited(true);
				if (it.image && !it.image->isSemaphorePersistent()) {
					fence.addRelease([img = it.image, sem = it.semaphore.get(), t = it.semaphore->getTimeline()] (bool success) {
						sem->setInUse(false, t);
						img->releaseSemaphore(sem);
					}, it.image, "DeviceQueue::submit::!isSemaphorePersistent");
				} else {
					fence.addRelease([sem = it.semaphore.get(), t = it.semaphore->getTimeline()] (bool success) {
						sem->setInUse(false, t);
					}, it.semaphore, "DeviceQueue::submit::isSemaphorePersistent");
				}
				fence.autorelease(it.semaphore.get());
				commandPool.autorelease(it.semaphore.get());
			}
		}

		for (auto &it : sync.signalAttachments) {
			if (it.semaphore) {
				it.semaphore->setSignaled(true);
				it.semaphore->setInUse(true, it.semaphore->getTimeline());
				fence.autorelease(it.semaphore.get());
				commandPool.autorelease(it.semaphore.get());
			}
		}

		fence.setArmed(*this);

		for (auto &it : sync.images) {
			it.image->setLayout(it.newLayout);
		}

		return true;
	}
	return false;
}

bool DeviceQueue::submit(Fence &fence, const CommandBuffer *buffer) {
	return submit(fence, makeSpanView(&buffer, 1));
}

bool DeviceQueue::submit(Fence &fence, SpanView<const CommandBuffer *> buffers) {
	Vector<VkCommandBuffer> vkBuffers; vkBuffers.reserve(buffers.size());

	for (auto &it : buffers) {
		if (it) {
			vkBuffers.emplace_back(it->getBuffer());
		}
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = 0;
	submitInfo.commandBufferCount = vkBuffers.size();
	submitInfo.pCommandBuffers = vkBuffers.data();
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

#ifdef XL_VKAPI_DEBUG
	[[maybe_unused]] auto frameIdx = _frameIdx;
	[[maybe_unused]] auto t = platform::device::_clock(platform::device::Monotonic);
	fence.addRelease([=] (bool success) {
		XL_VKAPI_LOG("[", frameIdx,  "] vkQueueSubmit [complete]",
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
	}, nullptr, "DeviceQueue::submit");
#endif

	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
#if XL_VKAPI_DEBUG
		auto t = platform::device::_clock(platform::device::Monotonic);
		_result = table.vkQueueSubmit(_queue, 1, &submitInfo, fence.getFence());
		XL_VKAPI_LOG("[", _frameIdx,  "] vkQueueSubmit: ", _result, " ", (void *)_queue,
				" [", platform::device::_clock(platform::device::Monotonic) - t, "]");
#else
		_result = table.vkQueueSubmit(_queue, 1, &submitInfo, fence.getFence());
#endif
	});

	if (_result == VK_SUCCESS) {
		fence.setArmed(*this);
		return true;
	}
	return false;
}

void DeviceQueue::waitIdle() {
	_device->makeApiCall([&] (const DeviceTable &table, VkDevice device) {
		table.vkQueueWaitIdle(_queue);
	});
}

uint32_t DeviceQueue::getActiveFencesCount() {
	return _nfences.load();
}

void DeviceQueue::retainFence(const Fence &fence) {
	++ _nfences;
}

void DeviceQueue::releaseFence(const Fence &fence) {
	-- _nfences;
}

void DeviceQueue::setOwner(FrameHandle &frame) {
	_frameIdx = frame.getOrder();
}

void DeviceQueue::reset() {
	_result = VK_ERROR_UNKNOWN;
	_frameIdx = 0;
}

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, VkAccessFlags src, VkAccessFlags dst,
		VkImageLayout old, VkImageLayout _new)
: srcAccessMask(src), dstAccessMask(dst), oldLayout(old), newLayout(_new)
, image(image), subresourceRange(VkImageSubresourceRange{
	image->getAspectMask(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS
}) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, VkAccessFlags src, VkAccessFlags dst,
		VkImageLayout old, VkImageLayout _new, VkImageSubresourceRange range)
: srcAccessMask(src), dstAccessMask(dst), oldLayout(old), newLayout(_new)
, image(image), subresourceRange(range) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, VkAccessFlags src, VkAccessFlags dst,
		VkImageLayout old, VkImageLayout _new, QueueFamilyTransfer transfer)
: srcAccessMask(src), dstAccessMask(dst), oldLayout(old), newLayout(_new), familyTransfer(transfer)
, image(image), subresourceRange(VkImageSubresourceRange{
	image->getAspectMask(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS
}) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, VkAccessFlags src, VkAccessFlags dst,
	 VkImageLayout old, VkImageLayout _new, QueueFamilyTransfer transfer, VkImageSubresourceRange range)
: srcAccessMask(src), dstAccessMask(dst), oldLayout(old), newLayout(_new), familyTransfer(transfer)
, image(image), subresourceRange(range) { }

ImageMemoryBarrier::ImageMemoryBarrier(const VkImageMemoryBarrier &barrier)
: srcAccessMask(barrier.srcAccessMask), dstAccessMask(barrier.dstAccessMask)
, oldLayout(barrier.oldLayout), newLayout(barrier.newLayout)
, familyTransfer(QueueFamilyTransfer{barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex})
, image(nullptr), subresourceRange(barrier.subresourceRange) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, VkAccessFlags src, VkAccessFlags dst)
: srcAccessMask(src), dstAccessMask(dst), buffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, VkAccessFlags src, VkAccessFlags dst,
		QueueFamilyTransfer transfer, VkDeviceSize offset, VkDeviceSize size)
: srcAccessMask(src), dstAccessMask(dst), familyTransfer(transfer),
  buffer(buf), offset(offset), size(size) { }

BufferMemoryBarrier::BufferMemoryBarrier(const VkBufferMemoryBarrier &barrier)
: srcAccessMask(barrier.srcAccessMask), dstAccessMask(barrier.dstAccessMask)
, familyTransfer(QueueFamilyTransfer{barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex})
, buffer(nullptr), offset(barrier.offset), size(barrier.size) { }

CommandBuffer::~CommandBuffer() {
	invalidate();
}

bool CommandBuffer::init(const CommandPool *pool, const DeviceTable *table, VkCommandBuffer buffer) {
	_pool = pool;
	_table = table;
	_buffer = buffer;
	return true;
}

void CommandBuffer::invalidate() {
	_buffer = VK_NULL_HANDLE;
}

void CommandBuffer::cmdPipelineBarrier(VkPipelineStageFlags srcFlags, VkPipelineStageFlags dstFlags, VkDependencyFlags deps,
		SpanView<ImageMemoryBarrier> imageBarriers) {
	Vector<VkImageMemoryBarrier> images; images.reserve(imageBarriers.size());
	for (auto &it : imageBarriers) {
		images.emplace_back(VkImageMemoryBarrier{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask,
			it.oldLayout, it.newLayout,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.image->getImage(), it.subresourceRange
		});
		addImage(it.image);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr, 0, nullptr, images.size(), images.data());
}

void CommandBuffer::cmdPipelineBarrier(VkPipelineStageFlags srcFlags, VkPipelineStageFlags dstFlags, VkDependencyFlags deps,
		SpanView<BufferMemoryBarrier> bufferBarriers) {
	Vector<VkBufferMemoryBarrier> buffers; buffers.reserve(bufferBarriers.size());
	for (auto &it : bufferBarriers) {
		buffers.emplace_back(VkBufferMemoryBarrier{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.buffer->getBuffer(), it.offset, it.size
		});
		addBuffer(it.buffer);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr, buffers.size(), buffers.data(), 0, nullptr);
}


void CommandBuffer::cmdPipelineBarrier(VkPipelineStageFlags srcFlags, VkPipelineStageFlags dstFlags, VkDependencyFlags deps,
		SpanView<BufferMemoryBarrier> bufferBarriers, SpanView<ImageMemoryBarrier> imageBarriers) {
	Vector<VkBufferMemoryBarrier> buffers; buffers.reserve(bufferBarriers.size());
	for (auto &it : bufferBarriers) {
		buffers.emplace_back(VkBufferMemoryBarrier{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.buffer->getBuffer(), it.offset, it.size
		});
		addBuffer(it.buffer);
	}

	Vector<VkImageMemoryBarrier> images; images.reserve(imageBarriers.size());
	for (auto &it : imageBarriers) {
		images.emplace_back(VkImageMemoryBarrier{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask,
			it.oldLayout, it.newLayout,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.image->getImage(), it.subresourceRange
		});
		addImage(it.image);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr,
			buffers.size(), buffers.data(), images.size(), images.data());
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst) {
	VkBufferCopy copy{0, 0, std::min(src->getSize(), dst->getSize())};
	cmdCopyBuffer(src, dst, makeSpanView(&copy, 1));
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size) {
	VkBufferCopy copy{srcOffset, dstOffset, size};
	cmdCopyBuffer(src, dst, makeSpanView(&copy, 1));
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst, SpanView<VkBufferCopy> copy) {
	addBuffer(src);
	addBuffer(dst);

	_table->vkCmdCopyBuffer(_buffer, src->getBuffer(), dst->getBuffer(), copy.size(), copy.data());
}

void CommandBuffer::cmdCopyImage(Image *src, VkImageLayout srcLayout, Image *dst, VkImageLayout dstLayout, VkFilter filter) {
	auto sourceExtent = src->getInfo().extent;
	auto targetExtent = dst->getInfo().extent;

	if (sourceExtent == targetExtent) {
		VkImageCopy copy{
			VkImageSubresourceLayers{src->getAspectMask(), 0, 0, src->getInfo().arrayLayers.get()},
			VkOffset3D{0, 0, 0},
			VkImageSubresourceLayers{dst->getAspectMask(), 0, 0, dst->getInfo().arrayLayers.get()},
			VkOffset3D{0, 0, 0},
			VkExtent3D{targetExtent.width, targetExtent.height, targetExtent.depth}
		};

		_table->vkCmdCopyImage(_buffer, src->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dst->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	} else {
		VkImageBlit blit{
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, src->getInfo().arrayLayers.get()},
			{ VkOffset3D{0, 0, 0}, VkOffset3D{int32_t(sourceExtent.width), int32_t(sourceExtent.height), int32_t(sourceExtent.depth)} },
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, dst->getInfo().arrayLayers.get()},
			{ VkOffset3D{0, 0, 0}, VkOffset3D{int32_t(targetExtent.width), int32_t(targetExtent.height), int32_t(targetExtent.depth)} },
		};

		_table->vkCmdBlitImage(_buffer, src->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dst->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);
	}
}

void CommandBuffer::cmdCopyImage(Image *src, VkImageLayout srcLayout, Image *dst, VkImageLayout dstLayout, const VkImageCopy &copy) {
	addImage(src);
	addImage(dst);

	_table->vkCmdCopyImage(_buffer, src->getImage(), srcLayout, dst->getImage(), dstLayout, 1, &copy);
}

void CommandBuffer::cmdCopyImage(Image *src, VkImageLayout srcLayout, Image *dst, VkImageLayout dstLayout, SpanView<VkImageCopy> copy) {
	addImage(src);
	addImage(dst);

	_table->vkCmdCopyImage(_buffer, src->getImage(), srcLayout, dst->getImage(), dstLayout, copy.size(), copy.data());
}

void CommandBuffer::cmdCopyBufferToImage(Buffer *buf, Image *img, VkImageLayout layout, VkDeviceSize offset) {
	auto &extent = img->getInfo().extent;
	VkImageSubresourceLayers copyLayers({ img->getAspectMask(), 0, 0, img->getInfo().arrayLayers.get() });

	VkBufferImageCopy copyRegion{offset, 0, 0, copyLayers, VkOffset3D{0, 0, 0},
		VkExtent3D{extent.width, extent.height, extent.depth}};

	cmdCopyBufferToImage(buf, img, layout, makeSpanView(&copyRegion, 1));
}

void CommandBuffer::cmdCopyBufferToImage(Buffer *buf, Image *img, VkImageLayout layout, SpanView<VkBufferImageCopy> copy) {
	addBuffer(buf);
	addImage(img);

	_table->vkCmdCopyBufferToImage(_buffer, buf->getBuffer(), img->getImage(), layout, copy.size(), copy.data());
}

void CommandBuffer::cmdCopyImageToBuffer(Image *img, VkImageLayout layout, Buffer *buf, VkDeviceSize offset) {
	auto &extent = img->getInfo().extent;
	VkImageSubresourceLayers copyLayers({ img->getAspectMask(), 0, 0, img->getInfo().arrayLayers.get() });

	VkBufferImageCopy copyRegion{offset, 0, 0, copyLayers, VkOffset3D{0, 0, 0},
		VkExtent3D{extent.width, extent.height, extent.depth}};

	cmdCopyImageToBuffer(img, layout, buf, makeSpanView(&copyRegion, 1));
}

void CommandBuffer::cmdCopyImageToBuffer(Image *img, VkImageLayout layout, Buffer *buf, SpanView<VkBufferImageCopy> copy) {
	addBuffer(buf);
	addImage(img);

	_table->vkCmdCopyImageToBuffer(_buffer, img->getImage(), layout, buf->getBuffer(), copy.size(), copy.data());
}

void CommandBuffer::cmdClearColorImage(Image *image, VkImageLayout layout, const Color4F &color) {
	VkClearColorValue clearColorEmpty;
	clearColorEmpty.float32[0] = color.r;
	clearColorEmpty.float32[1] = color.g;
	clearColorEmpty.float32[2] = color.b;
	clearColorEmpty.float32[3] = color.a;

	VkImageSubresourceRange range{ image->getAspectMask(), 0, image->getInfo().mipLevels.get(), 0, image->getInfo().arrayLayers.get() };

	addImage(image);
	_table->vkCmdClearColorImage(_buffer, image->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			&clearColorEmpty, 1, &range);
}

void CommandBuffer::cmdBeginRenderPass(RenderPassImpl *pass, Framebuffer *fb, VkSubpassContents subpass, bool alt) {
	auto &clearValues = pass->getClearValues();
	auto currentExtent = fb->getExtent();

	VkRenderPassBeginInfo renderPassInfo {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr
	};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.renderPass = pass->getRenderPass(alt);
	renderPassInfo.framebuffer = fb->getFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VkExtent2D{currentExtent.width, currentExtent.height};
	renderPassInfo.clearValueCount = clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();

	_framebuffers.emplace(fb);
	_table->vkCmdBeginRenderPass(_buffer, &renderPassInfo, subpass);
}

void CommandBuffer::cmdEndRenderPass() {
	_table->vkCmdEndRenderPass(_buffer);
}

void CommandBuffer::cmdSetViewport(uint32_t firstViewport, SpanView<VkViewport> viewports) {
	_table->vkCmdSetViewport(_buffer, firstViewport, viewports.size(), viewports.data());
}

void CommandBuffer::cmdSetScissor(uint32_t firstScissor, SpanView<VkRect2D> scissors) {
	_table->vkCmdSetScissor(_buffer, firstScissor, scissors.size(), scissors.data());
}

void CommandBuffer::cmdBindPipeline(GraphicPipeline *pipeline) {
	_table->vkCmdBindPipeline(_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
}

void CommandBuffer::cmdBindPipeline(ComputePipeline *pipeline) {
	_table->vkCmdBindPipeline(_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getPipeline());
}

void CommandBuffer::cmdBindIndexBuffer(Buffer *buf, VkDeviceSize offset, VkIndexType indexType) {
	_table->vkCmdBindIndexBuffer(_buffer, buf->getBuffer(), offset, indexType);
}

void CommandBuffer::cmdBindDescriptorSets(RenderPassImpl *pass, uint32_t firstSet) {
	auto &sets = pass->getDescriptorSets();

	Vector<VkDescriptorSet> bindSets; bindSets.reserve(sets.size());
	for (auto &it : sets) {
		bindSets.emplace_back(it->set);
		_descriptorSets.emplace(it);
	}

	cmdBindDescriptorSets(pass, bindSets, firstSet);
}

void CommandBuffer::cmdBindDescriptorSets(RenderPassImpl *pass, SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	auto bindPoint = (pass->getType() == gl::RenderPassType::Compute) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;

	_table->vkCmdBindDescriptorSets(_buffer, bindPoint, pass->getPipelineLayout(), firstSet,
			sets.size(), sets.data(), 0, nullptr);
}

void CommandBuffer::cmdBindGraphicDescriptorSets(VkPipelineLayout layout, SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	_table->vkCmdBindDescriptorSets(_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, firstSet,
			sets.size(), sets.data(), 0, nullptr);
}

void CommandBuffer::cmdBindComputeDescriptorSets(VkPipelineLayout layout, SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	_table->vkCmdBindDescriptorSets(_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, firstSet,
			sets.size(), sets.data(), 0, nullptr);
}

void CommandBuffer::cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
	_table->vkCmdDraw(_buffer, vertexCount, instanceCount, firstVertex, firstInstance);
}
void CommandBuffer::cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
		int32_t vertexOffset, uint32_t firstInstance) {
	_table->vkCmdDrawIndexed(_buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::cmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, BytesView data) {
	_table->vkCmdPushConstants(_buffer, layout, stageFlags, offset, data.size(), data.data());
}

void CommandBuffer::cmdFillBuffer(Buffer *buffer, uint32_t data) {
	cmdFillBuffer(buffer, 0, VK_WHOLE_SIZE, data);
}

void CommandBuffer::cmdFillBuffer(Buffer *buffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
	addBuffer(buffer);
	_table->vkCmdFillBuffer(_buffer, buffer->getBuffer(), dstOffset, size, data);
}

void CommandBuffer::cmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
	_table->vkCmdDispatch(_buffer, groupCountX, groupCountY, groupCountZ);
}

void CommandBuffer::cmdNextSubpass() {
	_table->vkCmdNextSubpass(_buffer, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::addImage(Image *image) {
	_images.emplace(image);
}

void CommandBuffer::addBuffer(Buffer *buffer) {
	_buffers.emplace(buffer);
	if (auto pool = buffer->getPool()) {
		_memPool.emplace(pool);
	}
}

CommandPool::~CommandPool() {
	if (_commandPool) {
		log::vtext("VK-Error", "CommandPool was not destroyed");
	}
}

bool CommandPool::init(Device &dev, uint32_t familyIdx, QueueOperations c, bool transient) {
	_familyIdx = familyIdx;
	_class = c;
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.queueFamilyIndex = familyIdx;
	poolInfo.flags = 0; // transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;

	return dev.getTable()->vkCreateCommandPool(dev.getDevice(), &poolInfo, nullptr, &_commandPool) == VK_SUCCESS;
}

void CommandPool::invalidate(Device &dev) {
	if (_commandPool) {
		dev.getTable()->vkDestroyCommandPool(dev.getDevice(), _commandPool, nullptr);
		_commandPool = VK_NULL_HANDLE;
	} else {
		log::vtext("VK-Error", "CommandPool is not defined");
	}
}

const CommandBuffer *CommandPool::recordBuffer(Device &dev, const Callback<bool(CommandBuffer &)> &cb,
		VkCommandBufferUsageFlagBits flags, Level level) {
	if (!_commandPool) {
		return nullptr;
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = _commandPool;
	allocInfo.level = VkCommandBufferLevel(level);
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer buf;
	if (dev.getTable()->vkAllocateCommandBuffers(dev.getDevice(), &allocInfo, &buf) != VK_SUCCESS) {
		return nullptr;
	}


	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = flags;
	beginInfo.pInheritanceInfo = nullptr;

	if (dev.getTable()->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	auto b = Rc<CommandBuffer>::create(this, dev.getTable(), buf);
	if (!b) {
		dev.getTable()->vkEndCommandBuffer(buf);
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	auto result = cb(*b);

	dev.getTable()->vkEndCommandBuffer(buf);

	if (!result) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	return _buffers.emplace_back(move(b)).get();
}

void CommandPool::freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &vec) {
	if (_commandPool) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, static_cast<uint32_t>(vec.size()), vec.data());
	}
	vec.clear();
}

void CommandPool::reset(Device &dev, bool release) {
	if (_commandPool) {
		//dev.getTable()->vkResetCommandPool(dev.getDevice(), _commandPool, release ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0);

		Vector<VkCommandBuffer> buffersToFree;
		for (auto &it : _buffers) {
			if (it) {
				buffersToFree.emplace_back(it->getBuffer());
			}
		}
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, static_cast<uint32_t>(buffersToFree.size()), buffersToFree.data());
		dev.getTable()->vkDestroyCommandPool(dev.getDevice(), _commandPool, nullptr);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.queueFamilyIndex = _familyIdx;
		poolInfo.flags = 0; // transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;

		dev.getTable()->vkCreateCommandPool(dev.getDevice(), &poolInfo, nullptr, &_commandPool);

		_buffers.clear();
		_autorelease.clear();
	}
}

void CommandPool::autorelease(Rc<Ref> &&ref) {
	_autorelease.emplace_back(move(ref));
}

}
