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

#ifndef XENOLITH_GL_VK_XLVKALLOCATOR_H_
#define XENOLITH_GL_VK_XLVKALLOCATOR_H_

#include "XLVkInfo.h"
#include "XLVkObject.h"

namespace stappler::xenolith::vk {

class Device;
class DeviceBuffer;

enum class AllocationUsage {
	DeviceLocal, // device local only
	DeviceLocalHostVisible, // device local, visible directly on host
	HostTransitionSource, // host-local, used as source for transfer to GPU device (so, non-cached, coherent preferable)
	HostTransitionDestination, // host-local, used as destination for transfer from GPU Device (cached, non-coherent)
};

enum class AllocationType {
	Unknown,
	Linear,
	Optimal,
};

struct MemoryRequirements {
	bool prefersDedicated = false;
	bool requiresDedicated = false;
	VkMemoryRequirements requirements;
};

class Allocator : public Ref {
public:
	static constexpr uint64_t PageSize = 8_MiB;
	static constexpr uint64_t MaxIndex = 20; // Largest block

	enum MemHeapType {
		HostLocal,
		DeviceLocal,
		DeviceLocalHostVisible,
	};

	struct MemHeap;

	// slice of device memory
	struct MemNode {
		uint64_t index = 0; // size in pages
		VkDeviceMemory mem = VK_NULL_HANDLE; // device mem block
		VkDeviceSize size = 0; // size in bytes
		VkDeviceSize offset = 0;  // current usage offset
		AllocationType lastAllocation = AllocationType::Unknown; // last allocation type (for bufferImageGranularity)
		void *ptr = nullptr;

		operator bool () const { return mem != VK_NULL_HANDLE; }

		size_t getFreeSpace() const { return size - offset; }
	};

	// Memory block, allocated from node for suballocation
	struct MemBlock {
		VkDeviceMemory mem = VK_NULL_HANDLE; // device mem block
		VkDeviceSize offset = 0; // offset in block
		VkDeviceSize size = 0; // reserved size after offset
		uint32_t type = 0; // memory type index
		void *ptr = nullptr;

		operator bool () const { return mem != VK_NULL_HANDLE; }
	};

	struct MemType {
		uint32_t idx;
		VkMemoryType type;
		uint64_t min = 2; // in PageSize
		uint64_t last = 0; // largest used index into free
		uint64_t max = 20; // Pages to preserve, 0 - do not preserve
		uint64_t current = 20; // current allocated size in BOUNDARY_SIZE
		std::array<Vector<MemNode>, MaxIndex> buf;

		bool isDeviceLocal() const { return (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0; }
		bool isHostVisible() const { return (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0; }
		bool isHostCoherent() const { return (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0; }
		bool isHostCached() const { return (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0; }
		bool isLazilyAllocated() const { return (type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0; }
		bool isProtected() const { return (type.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0; }
	};

	struct MemHeap {
		uint32_t idx;
		VkMemoryHeap heap;
		Vector<MemType> types;
		MemHeapType type = HostLocal;
		VkDeviceSize budget = 0;
		VkDeviceSize usage = 0;
		VkDeviceSize currentUsage = 0;
	};

	virtual ~Allocator();

	bool init(Device &dev, VkPhysicalDevice device, const DeviceInfo::Features &features, const DeviceInfo::Properties &props);
	void invalidate(Device &dev);

	void update();

	uint32_t getInitialTypeMask() const;
	const Vector<MemHeap> &getMemHeaps() const { return _memHeaps; }
	Device *getDevice() const { return _device; }

	bool hasBudgetFeature() const { return _hasBudget; }
	bool hasMemReq2Feature() const { return _hasMemReq2; }
	bool hasDedicatedFeature() const { return _hasDedicated; }
	VkDeviceSize getBufferImageGranularity() const { return _bufferImageGranularity; }
	VkDeviceSize getNonCoherentAtomSize() const { return _nonCoherentAtomSize; }

	const MemType *getType(uint32_t) const;

	MemType * findMemoryType(uint32_t typeFilter, AllocationUsage) const;

	MemoryRequirements getMemoryRequirements(VkBuffer target);
	MemoryRequirements getMemoryRequirements(VkImage target);

	Rc<Buffer> spawnPersistent(AllocationUsage, const gl::BufferInfo &, BytesView = BytesView());
	Rc<Image> spawnPersistent(AllocationUsage, const gl::ImageInfo &, bool preinitialized, uint64_t forceId = 0);

protected:
	friend class DeviceMemoryPool;

	void lock();
	void unlock();

	MemNode alloc(MemType *, uint64_t, bool persistent = false);
	void free(MemType *, SpanView<MemNode>);

	Mutex _mutex;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	Device *_device = nullptr;
	VkPhysicalDeviceMemoryBudgetPropertiesEXT _memBudget = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
	VkPhysicalDeviceMemoryProperties2KHR _memProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR };
	Vector<MemHeap> _memHeaps;
	Vector<const MemType *> _memTypes;

	VkDeviceSize _bufferImageGranularity = 1;
	VkDeviceSize _nonCoherentAtomSize = 1;
	bool _hasBudget = false;
	bool _hasMemReq2 = false;
	bool _hasDedicated = false;
};

class DeviceMemoryPool : public Ref {
public:
	struct MemData {
		Allocator::MemType *type = nullptr;
		Vector<Allocator::MemNode> mem;
		Vector<Allocator::MemBlock> freed;
	};

	virtual ~DeviceMemoryPool();

	bool init(const Rc<Allocator> &, bool persistentMapping = false);

	Rc<DeviceBuffer> spawn(AllocationUsage type, const gl::BufferInfo &);
	Rc<Buffer> spawnPersistent(AllocationUsage, const gl::BufferInfo &);

	Device *getDevice() const;
	const Rc<Allocator> &getAllocator() const { return _allocator; }

protected:
	friend class DeviceBuffer;

	Allocator::MemBlock alloc(MemData *, VkDeviceSize size, VkDeviceSize alignment, AllocationType allocType);
	void free(Allocator::MemBlock &&);
	void clear(MemData *);

	bool _persistentMapping = false;
	Rc<Allocator> _allocator;
	Map<int64_t, MemData> _heaps;
	Vector<Rc<DeviceBuffer>> _buffers;
};

}

#endif /* XENOLITH_GL_VK_XLVKALLOCATOR_H_ */
