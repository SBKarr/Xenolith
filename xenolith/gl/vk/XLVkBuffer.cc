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
		if (_mapped) {
			dev.getTable()->vkUnmapMemory(dev.getDevice(), _memory.mem);
			_mapped = nullptr;
		}
		_pool->free(move(_memory));
		dev.getTable()->vkDestroyBuffer(dev.getDevice(), _buffer, nullptr);
		_buffer = VK_NULL_HANDLE;
	}
}

void DeviceBuffer::setPersistentMapping(bool value) {
	if (value != _persistentMapping) {
		_persistentMapping = value;
		if (!_persistentMapping && _mapped) {
			_pool->getDevice()->getTable()->vkUnmapMemory(_pool->getDevice()->getDevice(), _memory.mem);
			_mapped = nullptr;
		}
	}
}
bool DeviceBuffer::isPersistentMapping() const {
	return _persistentMapping;
}

bool DeviceBuffer::setData(BytesView data, VkDeviceSize offset) {
	auto size = std::min(_info.size - offset, data.size());

	auto t = _pool->getAllocator()->getType(_memory.type);
	if (!t->isHostVisible()) {
		return false;
	}

	void *mapped = nullptr;

	VkMappedMemoryRange range;
	range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.pNext = nullptr;
	range.memory = _memory.mem;
	range.offset = offset;
	range.size = (size == _info.size) ? VK_WHOLE_SIZE : size; // TODO: use VkPhysicalDeviceLimits::nonCoherentAtomSize

	if (_mapped) {
		mapped = _mapped;
	} else {
		if (_persistentMapping) {
			if (_pool->getDevice()->getTable()->vkMapMemory(_pool->getDevice()->getDevice(),
					_memory.mem, 0, _info.size, 0, &mapped) != VK_SUCCESS) {
				return false;
			}

			_mapped = mapped;
		} else {
			if (_pool->getDevice()->getTable()->vkMapMemory(_pool->getDevice()->getDevice(),
					_memory.mem, offset, size, 0, &mapped) != VK_SUCCESS) {
				return false;
			}
		}
	}

	if (!t->isHostCoherent() && _needInvalidate) {
		_needInvalidate = false;
		_pool->getDevice()->getTable()->vkInvalidateMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
	}

	if (_persistentMapping) {
		memcpy((uint8_t *)mapped + offset, data.data(), size);
	} else {
		memcpy(mapped, data.data(), size);
		/*base16::encode(std::cout, data);
		std::cout << "\n";*/
	}

	if (!t->isHostCoherent()) {
		_pool->getDevice()->getTable()->vkFlushMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
		_needInvalidate = true;
	}

	if (!_persistentMapping) {
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
	range.size = size;

	if (_mapped) {
		mapped = _mapped;
	} else {
		if (_persistentMapping) {
			if (_pool->getDevice()->getTable()->vkMapMemory(_pool->getDevice()->getDevice(),
					_memory.mem, 0, _info.size, 0, &mapped) == VK_SUCCESS) {
				return Bytes();
			}

			_mapped = mapped;
		} else {
			if (_pool->getDevice()->getTable()->vkMapMemory(_pool->getDevice()->getDevice(),
					_memory.mem, offset, size, 0, &mapped) == VK_SUCCESS) {
				return Bytes();
			}
		}
	}

	if (!t->isHostCoherent() && _needInvalidate) {
		_needInvalidate = false;
		_pool->getDevice()->getTable()->vkInvalidateMappedMemoryRanges(_pool->getDevice()->getDevice(), 1, &range);
	}

	Bytes ret; ret.resize(size);
	if (_persistentMapping) {
		memcpy(ret.data(), (char *)mapped + offset, size);
	} else {
		memcpy(ret.data(), mapped, size);
	}

	if (!_persistentMapping) {
		_pool->getDevice()->getTable()->vkUnmapMemory(_pool->getDevice()->getDevice(), _memory.mem);
	}
	return ret;
}

}
