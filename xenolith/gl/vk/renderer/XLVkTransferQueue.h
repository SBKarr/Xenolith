/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKTRANSFERQUEUE_H_
#define XENOLITH_GL_VK_RENDERER_XLVKTRANSFERQUEUE_H_

#include "XLVkQueuePass.h"
#include "XLVkAllocator.h"
#include "XLRenderQueueAttachment.h"

namespace stappler::xenolith::vk {

class DeviceQueue;
class CommandPool;
class TransferAttachment;

class TransferResource : public gl::AttachmentInputData {
public:
	struct BufferAllocInfo {
		gl::BufferData *data = nullptr;
		VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
		MemoryRequirements req;
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceSize offset = 0;
		VkDeviceSize stagingOffset = 0;
		VkDeviceMemory dedicated = VK_NULL_HANDLE;
		uint32_t dedicatedMemType = 0;
		std::optional<VkBufferMemoryBarrier> barrier;
		bool useStaging = false;

		BufferAllocInfo() = default;
		BufferAllocInfo(gl::BufferData *);
	};

	struct ImageAllocInfo {
		gl::ImageData *data = nullptr;
		VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr };
		MemoryRequirements req;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceSize offset = 0;
		VkDeviceSize stagingOffset = 0;
		VkDeviceMemory dedicated = VK_NULL_HANDLE;
		uint32_t dedicatedMemType = 0;
		std::optional<VkImageMemoryBarrier> barrier;
		bool useStaging = false;

		ImageAllocInfo() = default;
		ImageAllocInfo(gl::ImageData *);
	};

	struct StagingCopy {
		VkDeviceSize sourceOffet;
		VkDeviceSize sourceSize;
		ImageAllocInfo *targetImage = nullptr;
		BufferAllocInfo *targetBuffer = nullptr;
	};

	struct StagingBuffer : public Ref {
		virtual ~StagingBuffer() { }

		uint32_t memoryTypeIndex = maxOf<uint32_t>();
		BufferAllocInfo buffer;
		Vector<StagingCopy> copyData;
	};

	virtual ~TransferResource();
	void invalidate(Device &dev);

	bool init(const Rc<Allocator> &alloc, const Rc<gl::Resource> &, Function<void(bool)> &&cb = nullptr);
	bool init(const Rc<Allocator> &alloc, Rc<gl::Resource> &&, Function<void(bool)> &&cb = nullptr);

	bool initialize();
	bool compile();

	bool prepareCommands(uint32_t idx, VkCommandBuffer buf,
			Vector<VkImageMemoryBarrier> &outputImageBarriers, Vector<VkBufferMemoryBarrier> &outputBufferBarriers);
	bool transfer(const Rc<DeviceQueue> &, const Rc<CommandPool> &, const Rc<Fence> &);

	bool isValid() const { return _alloc != nullptr; }
	bool isStagingRequired() const { return !_stagingBuffer.copyData.empty(); }

protected:
	bool allocate();
	bool upload();

	bool allocateDedicated(const Rc<Allocator> &alloc, BufferAllocInfo &);
	bool allocateDedicated(const Rc<Allocator> &alloc, ImageAllocInfo &);

	size_t writeData(uint8_t *, BufferAllocInfo &);
	size_t writeData(uint8_t *, ImageAllocInfo &);

	// calculate offsets, size and transfer if no staging needed
	size_t preTransferData();

	bool createStagingBuffer(StagingBuffer &buffer, size_t) const;
	bool writeStaging(StagingBuffer &);
	void dropStaging(StagingBuffer &) const;

	Allocator::MemType * _memType = nullptr;
	VkDeviceSize _requiredMemory = 0;
	Rc<Allocator> _alloc;
	Rc<gl::Resource> _resource;
	VkDeviceMemory _memory = VK_NULL_HANDLE;
	Vector<BufferAllocInfo> _buffers;
	Vector<ImageAllocInfo> _images;
	VkDeviceSize _nonCoherentAtomSize = 1;
	StagingBuffer _stagingBuffer;
	Function<void(bool)> _callback;

	bool _initialized = false;
};

class TransferQueue : public renderqueue::Queue {
public:
	virtual ~TransferQueue();

	bool init();

	Rc<FrameRequest> makeRequest(Rc<TransferResource> &&);

protected:
	using renderqueue::Queue::init;

	TransferAttachment *_attachment = nullptr;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKTRANSFERQUEUE_H_ */
