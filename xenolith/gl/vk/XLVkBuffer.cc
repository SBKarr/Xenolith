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

#include "XLVkBuffer.h"
#include "XLVkDevice.h"

namespace stappler::xenolith::vk {

DeviceBuffer::~DeviceBuffer() {
	if (_buffer) {
		log::vtext("VK-Error", "Buffer was not destroyed");
	}
}

bool DeviceBuffer::init(DeviceMemoryPool *p, VkBuffer buf, Allocator::MemBlock &&mem, AllocationUsage usage, const gl::BufferInfo &info) {
	_pool = p;
	_buffer = buf;
	_memory = mem;
	_info = info;
	_usage = usage;
	return true;
}

void DeviceBuffer::invalidate(Device &dev) {
	if (_buffer) {
		_pool->free(move(_memory));
		dev.getTable()->vkDestroyBuffer(dev.getDevice(), _buffer, nullptr);
		_buffer = VK_NULL_HANDLE;
	}
}

bool DeviceBuffer::setData(BytesView data, VkDeviceSize offset) {
	auto size = std::min(size_t(_info.size - offset), data.size());

	auto t = _pool->getAllocator()->getType(_memory.type);
	if (!t->isHostVisible()) {
		return false;
	}

	std::unique_lock<Mutex> lock(_pool->getMutex());

	void *mapped = nullptr;

	VkMappedMemoryRange range;
	range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.pNext = nullptr;
	range.memory = _memory.mem;
	range.offset = offset;
	if (size == _memory.size) {
		range.size = VK_WHOLE_SIZE;
	} else if (!t->isHostCoherent()) {
		range.size = math::align<VkDeviceSize>(size, _pool->getAllocator()->getNonCoherentAtomSize());
	} else {
		range.size = size;
	}

	if (_memory.ptr) {
		mapped = (uint8_t *)_memory.ptr + _memory.offset + offset;
	} else {
		if (_pool->getDevice()->getTable()->vkMapMemory(_pool->getDevice()->getDevice(),
				_memory.mem, _memory.offset + offset, size, 0, &mapped) != VK_SUCCESS) {
			return false;
		}
	}

	if (!t->isHostCoherent() && _needInvalidate) {
		_needInvalidate = false;
		_pool->getDevice()->getTable()->vkInvalidateMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
	}

	memcpy(mapped, data.data(), size);

	if (!t->isHostCoherent()) {
		_pool->getDevice()->getTable()->vkFlushMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
		_needInvalidate = true;
	}

	if (!_memory.ptr) {
		_pool->getDevice()->getTable()->vkUnmapMemory(_pool->getDevice()->getDevice(), _memory.mem);
	}
	return true;
}

Bytes DeviceBuffer::getData(VkDeviceSize size, VkDeviceSize offset) {
	size = std::min(_info.size - offset, size);

	auto t = _pool->getAllocator()->getType(_memory.type);
	if (!t->isHostVisible()) {
		return Bytes();
	}

	void *mapped = nullptr;

	VkMappedMemoryRange range;
	range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.pNext = nullptr;
	range.memory = _memory.mem;
	range.offset = offset;
	if (size == _memory.size) {
		range.size = VK_WHOLE_SIZE;
	} else if (!t->isHostCoherent()) {
		range.size = math::align<VkDeviceSize>(size, _pool->getAllocator()->getNonCoherentAtomSize());
	} else {
		range.size = size;
	}

	if (_memory.ptr) {
		mapped = (uint8_t *)_memory.ptr + _memory.offset;
	} else {
		if (_pool->getDevice()->getTable()->vkMapMemory(_pool->getDevice()->getDevice(),
				_memory.mem, offset, size, 0, &mapped) == VK_SUCCESS) {
			return Bytes();
		}
	}

	if (!t->isHostCoherent() && _needInvalidate) {
		_needInvalidate = false;
		_pool->getDevice()->getTable()->vkInvalidateMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
	}

	Bytes ret; ret.resize(size);
	memcpy(ret.data(), mapped, size);

	if (!_memory.ptr) {
		_pool->getDevice()->getTable()->vkUnmapMemory(_pool->getDevice()->getDevice(), _memory.mem);
	}
	return ret;
}

DeviceBuffer::MappedRegion DeviceBuffer::map(VkDeviceSize offset, VkDeviceSize size, bool invalidate) {
	if (_memory.ptr) {
		return MappedRegion({(uint8_t *)_memory.ptr + _memory.offset + offset, offset,
			std::min(VkDeviceSize(_info.size - offset), size)});
	} else {
		auto t = _pool->getAllocator()->getType(_memory.type);
		if (!t->isHostVisible()) {
			return MappedRegion({nullptr});
		}

		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = nullptr;
		range.memory = _memory.mem;
		range.offset = offset;
		if (!t->isHostCoherent()) {
			size = math::align<VkDeviceSize>(size, _pool->getAllocator()->getNonCoherentAtomSize());
		}
		range.size = std::min(_info.size - offset, size);

		void *mapped = nullptr;
		if (_pool->getDevice()->getTable()->vkMapMemory(_pool->getDevice()->getDevice(),
				_memory.mem, _memory.offset + offset, size, 0, &mapped) != VK_SUCCESS) {
			return MappedRegion({nullptr});
		}

		if (!t->isHostCoherent() && (_needInvalidate || invalidate)) {
			_needInvalidate = false;
			_pool->getDevice()->getTable()->vkInvalidateMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
		}

		return MappedRegion({(uint8_t *)mapped, offset, range.size});
	}
}

void DeviceBuffer::unmap(const MappedRegion &region, bool flush) {
	if (_memory.ptr) {
		// persistent mapping, no need to unmap
		if (flush) {
			VkMappedMemoryRange range;
			range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			range.pNext = nullptr;
			range.memory = _memory.mem;
			range.offset = region.offset;
			range.size = math::align<VkDeviceSize>(region.size, _pool->getAllocator()->getNonCoherentAtomSize());;

			_pool->getDevice()->getTable()->vkFlushMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
			_needInvalidate = true;
		}
	} else {
		auto t = _pool->getAllocator()->getType(_memory.type);
		if (!t->isHostVisible()) {
			return;
		}

		if (!t->isHostCoherent() && flush) {
			VkMappedMemoryRange range;
			range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			range.pNext = nullptr;
			range.memory = _memory.mem;
			range.offset = region.offset;
			range.size = region.size;

			_pool->getDevice()->getTable()->vkFlushMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
			_needInvalidate = true;
		}

		_pool->getDevice()->getTable()->vkUnmapMemory(_pool->getDevice()->getDevice(), _memory.mem);
	}
}

uint64_t DeviceBuffer::reserveBlock(uint64_t blockSize, uint64_t alignment) {
	auto alignedSize = math::align(uint64_t(blockSize), alignment);
	auto ret = _targetOffset.fetch_add(alignedSize);
	if (ret + blockSize > _info.size) {
		return maxOf<uint64_t>();
	}
	return ret;
}

}
