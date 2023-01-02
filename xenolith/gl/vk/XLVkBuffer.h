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

#ifndef XENOLITH_GL_VK_XLVKBUFFER_H_
#define XENOLITH_GL_VK_XLVKBUFFER_H_

#include "XLVkAllocator.h"

namespace stappler::xenolith::vk {

class DeviceBuffer : public Ref {
public:
	struct MappedRegion {
		uint8_t *ptr;
		VkDeviceSize offset = 0;
		VkDeviceSize size = maxOf<VkDeviceSize>();
	};

	virtual ~DeviceBuffer();

	bool init(DeviceMemoryPool *, VkBuffer, Allocator::MemBlock &&, AllocationUsage usage, const gl::BufferInfo &);

	void invalidate(Device &dev);

	bool setData(BytesView, VkDeviceSize offset = 0);
	Bytes getData(VkDeviceSize size = maxOf<VkDeviceSize>(), VkDeviceSize offset = 0);

	MappedRegion map(VkDeviceSize offset = 0, VkDeviceSize size = maxOf<VkDeviceSize>(), bool invalidate = true);
	void unmap(const MappedRegion &, bool flush = true);

	VkBuffer getBuffer() const { return _buffer; }
	VkDeviceSize getSize() const { return _info.size; }
	const gl::BufferInfo & getUsage() const { return _info; }

	// returns maxOf<uint64_t>() on overflow
	uint64_t reserveBlock(uint64_t blockSize, uint64_t alignment);
	uint64_t getReservedSize() const { return _targetOffset.load(); }

	void setPendingBarrier(const VkBufferMemoryBarrier &);
	const VkBufferMemoryBarrier *getPendingBarrier() const;
	void dropPendingBarrier();

protected:
	std::atomic<uint64_t> _targetOffset = 0;
	AllocationUsage _usage = AllocationUsage::DeviceLocal;
	gl::BufferInfo _info;
	Allocator::MemBlock _memory;
	DeviceMemoryPool *_pool = nullptr;
	VkBuffer _buffer;
	bool _needInvalidate = false;
	std::optional<VkBufferMemoryBarrier> _barrier;
};

}

#endif /* XENOLITH_GL_VK_XLVKBUFFER_H_ */
