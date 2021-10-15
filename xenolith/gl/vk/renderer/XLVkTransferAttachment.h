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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKTRANSFERATTACHMENT_H_
#define XENOLITH_GL_VK_RENDERER_XLVKTRANSFERATTACHMENT_H_

#include "XLVkRenderPass.h"
#include "XLVkAllocator.h"
#include "XLGlAttachment.h"
#include <optional>

namespace stappler::xenolith::vk {

class DeviceQueue;
class CommandPool;

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
		size_t sourceOffet;
		size_t sourceSize;
		ImageAllocInfo *targetImage = nullptr;
		BufferAllocInfo *targetBuffer = nullptr;
	};

	struct StagingBuffer : public Ref {
		uint32_t memoryTypeIndex = maxOf<uint32_t>();
		BufferAllocInfo buffer;
		Vector<StagingCopy> copyData;
	};

	virtual ~TransferResource();
	void invalidate(Device &dev);

	bool init(const Rc<Allocator> &alloc, const Rc<gl::Resource> &, Function<void(bool)> &&cb = nullptr);

	bool initialize();
	bool compile();

	bool prepareCommands(uint32_t idx, VkCommandBuffer buf);
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
};

class TransferAttachment : public gl::GenericAttachment {
public:
	virtual ~TransferAttachment();

	virtual Rc<gl::AttachmentHandle> makeFrameHandle(const gl::FrameHandle &) override;
};

class TransferAttachmentHandle : public gl::AttachmentHandle {
public:
	virtual ~TransferAttachmentHandle();

	virtual bool setup(gl::FrameHandle &) override;
	virtual bool submitInput(gl::FrameHandle &, Rc<gl::AttachmentInputData> &&) override;

	const Rc<TransferResource> &getResource() const { return _resource; }

protected:
	Rc<TransferResource> _resource;
};

class TransferRenderPass : public RenderPass {
public:
	virtual ~TransferRenderPass();

	virtual bool init(StringView);

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(gl::RenderPassData *, const gl::FrameHandle &);
};

class TransferRenderPassHandle : public RenderPassHandle {
public:
	virtual ~TransferRenderPassHandle();

protected:
	virtual Vector<VkCommandBuffer> doPrepareCommands(gl::FrameHandle &, uint32_t index);
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKTRANSFERATTACHMENT_H_ */
