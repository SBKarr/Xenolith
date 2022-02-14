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

#include "XLVkAttachment.h"
#include "XLVkFrame.h"
#include "XLVkDevice.h"

namespace stappler::xenolith::vk {

BufferAttachmentHandle::~BufferAttachmentHandle() {

}

VertexBufferAttachment::~VertexBufferAttachment() {

}

Rc<gl::AttachmentHandle> VertexBufferAttachment::makeFrameHandle(const gl::FrameQueue &frame) {
	return Rc<VertexBufferAttachmentHandle>::create(this, frame);
}

VertexBufferAttachmentHandle::~VertexBufferAttachmentHandle() {
	if (_pool) {
		_device->releaseCommandPoolUnsafe(move(_pool));
		_pool = nullptr;
	}

	if (_fence) {
		_device->releaseFence(move(_fence));
		_fence = nullptr;
	}

	if (_transferQueue) {
		_device->releaseQueue(move(_transferQueue));
		_transferQueue = nullptr;
	}
}

void VertexBufferAttachmentHandle::submitInput(gl::FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	if (auto d = data.cast<gl::VertexData>()) {
		q.getFrame().performInQueue([this, d = move(d)] (gl::FrameHandle &handle) {
			return loadVertexes(handle, d);
		}, [this, cb = move(cb)] (gl::FrameHandle &handle, bool success) {
			cb(success);
		}, this, "VertexBufferAttachmentHandle::submitInput");
	} else {
		cb(false);
	}
}

bool VertexBufferAttachmentHandle::isDescriptorDirty(const gl::RenderPassHandle &, const gl::PipelineDescriptor &,
		uint32_t, bool isExternal) const {
	return true;
}

bool VertexBufferAttachmentHandle::writeDescriptor(const RenderPassHandle &, const gl::PipelineDescriptor &,
		uint32_t, bool, VkDescriptorBufferInfo &info) {
	info.buffer = _vertexes->getBuffer();
	info.offset = 0;
	info.range = _vertexes->getSize();
	return true;
}

bool VertexBufferAttachmentHandle::loadVertexes(gl::FrameHandle &fhandle, const Rc<gl::VertexData> &vertexes) {
	auto handle = dynamic_cast<FrameHandle *>(&fhandle);
	if (!handle) {
		return false;
	}

	_data = vertexes;

	auto &memPool = handle->getMemPool();

	_vertexes = memPool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::StorageBuffer, vertexes->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U)));
	_vertexes->setData(BytesView((uint8_t *)vertexes->data.data(), vertexes->data.size() * sizeof(gl::Vertex_V4F_V4F_T2F2U)));


	_indexes = memPool->spawn(AllocationUsage::DeviceLocalHostVisible,
			gl::BufferInfo(gl::BufferUsage::IndexBuffer, vertexes->indexes.size() * sizeof(uint32_t)));
	_indexes->setData(BytesView((uint8_t *)vertexes->indexes.data(), vertexes->indexes.size() * sizeof(uint32_t)));


	// index buffer
	/*_indexesStaging = handle->getMemPool()->spawn(AllocationUsage::HostTransitionSource,
			gl::BufferInfo(gl::BufferUsage::TransferSrc, vertexes->indexes.size() * sizeof(uint32_t)));
	_indexesStaging->setData(BytesView((uint8_t *)vertexes->indexes.data(), vertexes->indexes.size() * sizeof(uint32_t)));

	_indexes = handle->getMemPool()->spawn(AllocationUsage::DeviceLocal,
			gl::BufferInfo(gl::BufferUsage::IndexBuffer, vertexes->indexes.size() * sizeof(uint32_t)));

	auto buf = _pool->allocBuffer(*_device);
	auto table = _device->getTable();

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return false;
	}

	VkBufferCopy indexesCopy;
	indexesCopy.srcOffset = 0;
	indexesCopy.dstOffset = 0;
	indexesCopy.size = _indexesStaging->getSize();

	table->vkCmdCopyBuffer(buf, _indexesStaging->getBuffer(), _indexes->getBuffer(), 1, &indexesCopy);

	if (table->vkEndCommandBuffer(buf) != VK_SUCCESS) {
		return false;
	}

	_fence->addRelease([dev = _device, pool = move(_pool)] {
		dev->releaseCommandPool(Rc<CommandPool>(pool));
	});
	_pool = nullptr;

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	submitInfo.waitSemaphoreCount = 0;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &buf;
	submitInfo.signalSemaphoreCount = 0;

	if (table->vkQueueSubmit(_transferQueue->getQueue(), 1, &submitInfo, _fence->getFence()) != VK_SUCCESS) {
		return false;
	}*/

	return true;
}

}
