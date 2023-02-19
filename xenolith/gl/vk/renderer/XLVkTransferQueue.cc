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

#include "XLVkTransferQueue.h"
#include "XLVkDevice.h"
#include "XLVkObject.h"

namespace stappler::xenolith::vk {

class TransferAttachment : public renderqueue::GenericAttachment {
public:
	virtual ~TransferAttachment();

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class TransferAttachmentHandle : public renderqueue::AttachmentHandle {
public:
	virtual ~TransferAttachmentHandle();

	virtual bool setup(FrameQueue &, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	const Rc<TransferResource> &getResource() const { return _resource; }

protected:
	Rc<TransferResource> _resource;
};

class TransferRenderPass : public QueuePass {
public:
	virtual ~TransferRenderPass();

	virtual bool init(StringView);

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

	TransferAttachment *getAttachment() const { return _attachment; }

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	TransferAttachment *_attachment = nullptr;
};

class TransferRenderPassHandle : public QueuePassHandle {
public:
	virtual ~TransferRenderPassHandle();

protected:
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool) override;
};


TransferQueue::~TransferQueue() { }

bool TransferQueue::init() {
	renderqueue::Queue::Builder builder("Transfer");

	auto attachment = Rc<TransferAttachment>::create("TransferAttachment");
	auto pass = Rc<TransferRenderPass>::create("TransferRenderPass");

	builder.addRenderPass(pass);
	builder.addPassInput(pass, 0, attachment, renderqueue::AttachmentDependencyInfo());
	builder.addPassOutput(pass, 0, attachment, renderqueue::AttachmentDependencyInfo());
	builder.addInput(attachment);
	builder.addOutput(attachment);

	if (renderqueue::Queue::init(move(builder))) {
		_attachment = attachment;
		return true;
	}
	return false;
}

auto TransferQueue::makeRequest(Rc<TransferResource> &&req)-> Rc<FrameRequest> {
	auto ret = Rc<FrameRequest>::create(this);
	ret->addInput(_attachment, move(req));
	return ret;
}

TransferResource::BufferAllocInfo::BufferAllocInfo(gl::BufferData *d) {
	data = d;
	info.flags = VkBufferCreateFlags(d->flags);
	info.size = d->size;
	info.usage = VkBufferUsageFlags(d->usage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
}

TransferResource::ImageAllocInfo::ImageAllocInfo(gl::ImageData *d) {
	data = d;
	info.flags = data->flags;
	info.imageType = VkImageType(data->imageType);
	info.format = VkFormat(data->format);
	info.extent = VkExtent3D({ data->extent.width, data->extent.height, data->extent.depth });
	info.mipLevels = data->mipLevels.get();
	info.arrayLayers = data->arrayLayers.get();
	info.samples = VkSampleCountFlagBits(data->samples);
	info.tiling = VkImageTiling(data->tiling);
	info.usage = VkImageUsageFlags(data->usage) | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (data->tiling == gl::ImageTiling::Optimal) {
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	} else {
		info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	}
}

TransferResource::~TransferResource() {
	if (_alloc) {
		invalidate(*_alloc->getDevice());
	}
}

void TransferResource::invalidate(Device &dev) {
	for (auto &it : _buffers) {
		if (it.buffer != VK_NULL_HANDLE) {
			dev.getTable()->vkDestroyBuffer(dev.getDevice(), it.buffer, nullptr);
			it.buffer = VK_NULL_HANDLE;
		}
		if (it.dedicated != VK_NULL_HANDLE) {
			dev.getTable()->vkFreeMemory(dev.getDevice(), it.dedicated, nullptr);
			it.dedicated = VK_NULL_HANDLE;
		}
	}
	for (auto &it : _images) {
		if (it.image != VK_NULL_HANDLE) {
			dev.getTable()->vkDestroyImage(dev.getDevice(), it.image, nullptr);
			it.image = VK_NULL_HANDLE;
		}
		if (it.dedicated != VK_NULL_HANDLE) {
			dev.getTable()->vkFreeMemory(dev.getDevice(), it.dedicated, nullptr);
			it.dedicated = VK_NULL_HANDLE;
		}
	}
	if (_memory != VK_NULL_HANDLE) {
		dev.getTable()->vkFreeMemory(dev.getDevice(), _memory, nullptr);
		_memory = VK_NULL_HANDLE;
	}

	dropStaging(_stagingBuffer);

	if (_callback) {
		_callback(false);
		_callback = nullptr;
	}

	_memType = nullptr;
	_alloc = nullptr;
}

bool TransferResource::init(const Rc<Allocator> &alloc, const Rc<gl::Resource> &res, Function<void(bool)> &&cb) {
	_alloc = alloc;
	_resource = res;
	if (cb) {
		_callback = move(cb);
	}
	return true;
}

bool TransferResource::init(const Rc<Allocator> &alloc, Rc<gl::Resource> &&res, Function<void(bool)> &&cb) {
	_alloc = alloc;
	_resource = move(res);
	if (cb) {
		_callback = move(cb);
	}
	return true;
}

bool TransferResource::initialize() {
	auto dev = _alloc->getDevice();
	auto table = _alloc->getDevice()->getTable();

	auto cleanup = [&] (StringView reason) {
		_resource->clear();
		invalidate(*_alloc->getDevice());
		log::vtext("DeviceResourceTransfer", "Fail to init transfer for ", _resource->getName(), ": ", reason);
		return false;
	};

	_buffers.reserve(_resource->getBuffers().size());
	_images.reserve(_resource->getImages().size());

	for (auto &it : _resource->getBuffers()) {
		_buffers.emplace_back(it);
	}

	for (auto &it : _resource->getImages()) {
		_images.emplace_back(it);
	}

	// pre-create objects
	auto mask = _alloc->getInitialTypeMask();
	for (auto &it : _buffers) {
		if (table->vkCreateBuffer(dev->getDevice(), &it.info, nullptr, &it.buffer) != VK_SUCCESS) {
			return cleanup("Fail to create buffer");
		}

		it.req = _alloc->getBufferMemoryRequirements(it.buffer);
		if (!it.req.prefersDedicated && !it.req.requiresDedicated) {
			mask &= it.req.requirements.memoryTypeBits;
		}
		if (mask == 0) {
			return cleanup("No memory type available");
		}
	}

	for (auto &it : _images) {
		if (table->vkCreateImage(dev->getDevice(), &it.info, nullptr, &it.image) != VK_SUCCESS) {
			return cleanup("Fail to create image");
		}

		it.req = _alloc->getImageMemoryRequirements(it.image);
		if (!it.req.prefersDedicated && !it.req.requiresDedicated) {
			mask &= it.req.requirements.memoryTypeBits;
		}
		if (mask == 0) {
			return cleanup("No memory type available");
		}
	}

	if (mask == 0) {
		return cleanup("No common memory type for resource found");
	}

	auto allocMemType = _alloc->findMemoryType(mask, AllocationUsage::DeviceLocal);

	if (!allocMemType) {
		log::vtext("Vk-Error", "Fail to find memory type for static resource: ", _resource->getName());
		return cleanup("Memory type not found");
	}

	if (allocMemType->isHostVisible()) {
		if (!allocMemType->isHostCoherent()) {
			_nonCoherentAtomSize = _alloc->getNonCoherentAtomSize();
		}
	}

	for (auto &it : _images) {
		if (!it.req.requiresDedicated && !it.req.prefersDedicated) {
			if (it.info.tiling == VK_IMAGE_TILING_OPTIMAL) {
				_requiredMemory = math::align<VkDeviceSize>(_requiredMemory,
						std::max(it.req.requirements.alignment, _nonCoherentAtomSize));
				it.offset = _requiredMemory;
				_requiredMemory += it.req.requirements.size;
			}
		}
	}

	_requiredMemory = math::align<VkDeviceSize>(_requiredMemory, _alloc->getBufferImageGranularity());

	for (auto &it : _images) {
		if (!it.req.requiresDedicated && !it.req.prefersDedicated) {
			if (it.info.tiling != VK_IMAGE_TILING_OPTIMAL) {
				_requiredMemory = math::align<VkDeviceSize>(_requiredMemory,
						std::max(it.req.requirements.alignment, _nonCoherentAtomSize));
				it.offset = _requiredMemory;
				_requiredMemory += it.req.requirements.size;
			}
		}
	}

	for (auto &it : _buffers) {
		if (!it.req.requiresDedicated && !it.req.prefersDedicated) {
			_requiredMemory += math::align<VkDeviceSize>(_requiredMemory,
					std::max(it.req.requirements.alignment, _nonCoherentAtomSize));
			it.offset = _requiredMemory;
			_requiredMemory += it.req.requirements.size;
		}
	}

	_memType = allocMemType;

	return allocate() && upload();
}

bool TransferResource::allocate() {
	if (!_memType) {
		return false;
	}

	auto dev = _alloc->getDevice();
	auto table = _alloc->getDevice()->getTable();

	auto cleanup = [&] (StringView reason) {
		invalidate(*_alloc->getDevice());
		log::vtext("DeviceResourceTransfer", "Fail to allocate memory for ", _resource->getName(), ": ", reason);
		return false;
	};

	if (_requiredMemory > 0) {
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = _requiredMemory;
		allocInfo.memoryTypeIndex = _memType->idx;

		if (table->vkAllocateMemory(dev->getDevice(), &allocInfo, nullptr, &_memory) != VK_SUCCESS) {
			log::vtext("Vk-Error", "Fail to allocate memory for static resource: ", _resource->getName());
			return cleanup("Fail to allocate memory");
		}
	}

	// bind memory
	for (auto &it : _images) {
		if (it.req.requiresDedicated || it.req.prefersDedicated) {
			if (!allocateDedicated(_alloc, it)) {
				return cleanup("Fail to allocate memory");
			}
		} else {
			if (it.info.tiling == VK_IMAGE_TILING_OPTIMAL) {
				table->vkBindImageMemory(dev->getDevice(), it.image, _memory, it.offset);
			}
		}
	}

	for (auto &it : _images) {
		if (!it.req.requiresDedicated && !it.req.prefersDedicated) {
			if (it.info.tiling != VK_IMAGE_TILING_OPTIMAL) {
				table->vkBindImageMemory(dev->getDevice(), it.image, _memory, it.offset);
			}
		}
	}

	for (auto &it : _buffers) {
		if (it.req.requiresDedicated || it.req.prefersDedicated) {
			if (!allocateDedicated(_alloc, it)) {
				return cleanup("Fail to allocate memory");
			}
		} else {
			table->vkBindBufferMemory(dev->getDevice(), it.buffer, _memory, it.offset);
		}
	}

	return true;
}

bool TransferResource::upload() {
	size_t stagingSize = preTransferData();
	if (stagingSize == 0) {
		return true;
	}

	if (stagingSize == maxOf<size_t>()) {
		invalidate(*_alloc->getDevice());
		return false; // failed with error
	}

	if (createStagingBuffer(_stagingBuffer, stagingSize)) {
		if (writeStaging(_stagingBuffer)) {
			return true;
		}
	}

	dropStaging(_stagingBuffer);
	invalidate(*_alloc->getDevice());
	return false;
}

bool TransferResource::compile() {
	Rc<DeviceMemory> mem;
	if (_memory) {
		mem = Rc<DeviceMemory>::create(*_alloc->getDevice(), _memory);
	}

	for (auto &it : _images) {
		Rc<Image> img;
		if (it.dedicated) {
			auto dedicated = Rc<DeviceMemory>::create(*_alloc->getDevice(), it.dedicated);
			img = Rc<Image>::create(*_alloc->getDevice(), it.image, *it.data, move(dedicated), Rc<gl::ImageAtlas>(it.data->atlas));
			it.dedicated = VK_NULL_HANDLE;
		} else {
			img = Rc<Image>::create(*_alloc->getDevice(), it.image, *it.data, Rc<DeviceMemory>(mem), Rc<gl::ImageAtlas>(it.data->atlas));
		}
		if (it.barrier) {
			img->setPendingBarrier(it.barrier.value());
		}
		it.data->image.set(img);
		it.image = VK_NULL_HANDLE;
	}

	for (auto &it : _buffers) {
		Rc<Buffer> buf;
		if (it.dedicated) {
			auto dedicated = Rc<DeviceMemory>::create(*_alloc->getDevice(), it.dedicated);
			buf = Rc<Buffer>::create(*_alloc->getDevice(), it.buffer, *it.data, move(dedicated));
			it.dedicated = VK_NULL_HANDLE;
		} else {
			buf = Rc<Buffer>::create(*_alloc->getDevice(), it.buffer, *it.data, Rc<DeviceMemory>(mem));
		}
		if (it.barrier) {
			buf->setPendingBarrier(it.barrier.value());
		}
		it.data->buffer.set(buf);
		it.buffer = VK_NULL_HANDLE;
	}

	_memory = VK_NULL_HANDLE;
	if (_callback) {
		_callback(true);
		_callback = nullptr;
	}

	return true;
}

static VkImageAspectFlagBits getFormatAspectFlags(VkFormat fmt, bool separateDepthStencil) {
	switch (fmt) {
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_X8_D24_UNORM_PACK32:
	case VK_FORMAT_D32_SFLOAT:
		if (separateDepthStencil) {
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		} else {
			return VkImageAspectFlagBits(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		}
		break;
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return VkImageAspectFlagBits(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		break;
	case VK_FORMAT_S8_UINT:
		if (separateDepthStencil) {
			return VK_IMAGE_ASPECT_STENCIL_BIT;
		} else {
			return VkImageAspectFlagBits(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
		}
		break;
	default:
		return VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	}
}

bool TransferResource::prepareCommands(uint32_t idx, VkCommandBuffer buf,
		Vector<VkImageMemoryBarrier> &outputImageBarriers, Vector<VkBufferMemoryBarrier> &outputBufferBarriers) {
	auto dev = _alloc->getDevice();
	auto table = _alloc->getDevice()->getTable();

	Vector<VkImageMemoryBarrier> inputImageBarriers;
	for (auto &it : _stagingBuffer.copyData) {
		if (it.targetImage) {
			inputImageBarriers.emplace_back(VkImageMemoryBarrier({
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
				VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				it.targetImage->image, VkImageSubresourceRange({
					getFormatAspectFlags(it.targetImage->info.format, false),
					0, it.targetImage->data->mipLevels.get(), 0, it.targetImage->data->arrayLayers.get()
				})
			}));
		}
	}

	table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr, // bufferBarriers.size(), bufferBarriers.data(),
			inputImageBarriers.size(), inputImageBarriers.data());

	for (auto &it : _stagingBuffer.copyData) {
		if (it.targetBuffer) {
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = it.sourceOffet;
			copyRegion.dstOffset = 0;
			copyRegion.size = it.sourceSize;
			table->vkCmdCopyBuffer(buf, _stagingBuffer.buffer.buffer, it.targetBuffer->buffer, 1, &copyRegion);
		} else if (it.targetImage) {
			VkBufferImageCopy copyRegion{};
			copyRegion.bufferOffset = it.sourceOffet;
			copyRegion.bufferRowLength = 0; // If either of these values is zero, that aspect of the buffer memory
			copyRegion.bufferImageHeight = 0; // is considered to be tightly packed according to the imageExtent
			copyRegion.imageSubresource = VkImageSubresourceLayers({
				getFormatAspectFlags(it.targetImage->info.format, false), 0, 0, it.targetImage->data->arrayLayers.get()
			});
			copyRegion.imageOffset = VkOffset3D({0, 0, 0});
			copyRegion.imageExtent = it.targetImage->info.extent;

			table->vkCmdCopyBufferToImage(buf, _stagingBuffer.buffer.buffer, it.targetImage->image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		}
	}

	for (auto &it : _stagingBuffer.copyData) {
		if (it.targetImage) {
			if (auto q = dev->getQueueFamily(getQueueOperations(it.targetImage->data->type))) {
				uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

				if (q->index != idx) {
					srcQueueFamilyIndex = idx;
					dstQueueFamilyIndex = q->index;
				}

				auto &ref = outputImageBarriers.emplace_back(VkImageMemoryBarrier({
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					srcQueueFamilyIndex, dstQueueFamilyIndex,
					it.targetImage->image, VkImageSubresourceRange({
						getFormatAspectFlags(it.targetImage->info.format, false),
						0, it.targetImage->data->mipLevels.get(), 0, it.targetImage->data->arrayLayers.get()
					})
				}));

				if (q->index != idx) {
					it.targetImage->barrier.emplace(ref);
				}
			}
		} else if (it.targetBuffer) {
			if (auto q = dev->getQueueFamily(getQueueOperations(it.targetImage->data->type))) {
				uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

				if (q->index != idx) {
					srcQueueFamilyIndex = idx;
					dstQueueFamilyIndex = q->index;
				}

				auto &ref = outputBufferBarriers.emplace_back(VkBufferMemoryBarrier({
					VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					srcQueueFamilyIndex, dstQueueFamilyIndex,
					it.targetBuffer->buffer, 0, VK_WHOLE_SIZE
				}));

				if (q->index != idx) {
					it.targetBuffer->barrier.emplace(ref);
				}
			}
		}
	}

	return true;
}

bool TransferResource::transfer(const Rc<DeviceQueue> &queue, const Rc<CommandPool> &pool, const Rc<Fence> &fence) {
	auto dev = _alloc->getDevice();
	auto table = _alloc->getDevice()->getTable();
	auto buf = pool->recordBuffer(*dev, [&] (CommandBuffer &buf) {
		Vector<VkImageMemoryBarrier> outputImageBarriers;
		Vector<VkBufferMemoryBarrier> outputBufferBarriers;

		if (!prepareCommands(queue->getIndex(), buf.getBuffer(), outputImageBarriers, outputBufferBarriers)) {
			return false;
		}

		table->vkCmdPipelineBarrier(buf.getBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
			0, nullptr,
			outputBufferBarriers.size(), outputBufferBarriers.data(),
			outputImageBarriers.size(), outputImageBarriers.data());
		return true;
	});

	if (buf) {
		return queue->submit(*fence, buf);
	}
	return false;
}

void TransferResource::dropStaging(StagingBuffer &buffer) const {
	auto dev = _alloc->getDevice();
	auto table = _alloc->getDevice()->getTable();

	if (buffer.buffer.buffer != VK_NULL_HANDLE) {
		table->vkDestroyBuffer(dev->getDevice(), buffer.buffer.buffer, nullptr);
		buffer.buffer.buffer = VK_NULL_HANDLE;
	}
	if (buffer.buffer.dedicated != VK_NULL_HANDLE) {
		table->vkFreeMemory(dev->getDevice(), buffer.buffer.dedicated, nullptr);
		buffer.buffer.dedicated = VK_NULL_HANDLE;
	}
}

bool TransferResource::allocateDedicated(const Rc<Allocator> &alloc, BufferAllocInfo &it) {
	auto dev = alloc->getDevice();
	auto table = alloc->getDevice()->getTable();
	auto type = alloc->findMemoryType(it.req.requirements.memoryTypeBits, AllocationUsage::DeviceLocal);
	if (!type) {
		return false;
	}

	VkMemoryDedicatedAllocateInfo dedicatedInfo;
	dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	dedicatedInfo.pNext = nullptr;
	dedicatedInfo.image = VK_NULL_HANDLE;
	dedicatedInfo.buffer = it.buffer;

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = &dedicatedInfo;
	allocInfo.allocationSize = it.req.requirements.size;
	allocInfo.memoryTypeIndex = type->idx;

	if (table->vkAllocateMemory(dev->getDevice(), &allocInfo, nullptr, &it.dedicated) != VK_SUCCESS) {
		log::vtext("Vk-Error", "Fail to allocate memory for static resource: ", _resource->getName());
		return false;
	}

	table->vkBindBufferMemory(dev->getDevice(), it.buffer, it.dedicated, 0);
	it.dedicatedMemType = type->idx;
	return true;
}

bool TransferResource::allocateDedicated(const Rc<Allocator> &alloc, ImageAllocInfo &it) {
	auto dev = alloc->getDevice();
	auto table = alloc->getDevice()->getTable();
	auto type = alloc->findMemoryType(it.req.requirements.memoryTypeBits, AllocationUsage::DeviceLocal);
	if (!type) {
		return false;
	}

	VkMemoryDedicatedAllocateInfo dedicatedInfo;
	dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	dedicatedInfo.pNext = nullptr;
	dedicatedInfo.image = it.image;
	dedicatedInfo.buffer = VK_NULL_HANDLE;

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = &dedicatedInfo;
	allocInfo.allocationSize = it.req.requirements.size;
	allocInfo.memoryTypeIndex = type->idx;

	if (table->vkAllocateMemory(dev->getDevice(), &allocInfo, nullptr, &it.dedicated) != VK_SUCCESS) {
		log::vtext("Vk-Error", "Fail to allocate memory for static resource: ", _resource->getName());
		return false;
	}

	table->vkBindImageMemory(dev->getDevice(), it.image, it.dedicated, 0);
	it.dedicatedMemType = type->idx;
	return true;
}

size_t TransferResource::writeData(uint8_t *mem, BufferAllocInfo &info) {
	if (!info.data->data.empty()) {
		auto size = std::min(size_t(info.data->data.size()), size_t(info.data->size));
		memcpy(mem, info.data->data.data(), size);
		return size;
	} else if (info.data->callback) {
		size_t size = 0;
		info.data->callback([&] (BytesView data) {
			size = std::min(size_t(data.size()), size_t(info.data->size));
			memcpy(mem, data.data(), size);
		});
		return size;
	}
	return 0;
}

size_t TransferResource::writeData(uint8_t *mem, ImageAllocInfo &info) {
	if (!info.data->data.empty()) {
		auto size = info.data->data.size();
		memcpy(mem, info.data->data.data(), size);
		return size;
	} else if (info.data->memCallback) {
		size_t size = 0;
		info.data->memCallback([&] (BytesView data) {
			size = data.size();
			memcpy(mem, data.data(), size);
		});
		return size;
	} else if (info.data->stdCallback) {
		size_t size = 0;
		info.data->stdCallback([&] (BytesView data) {
			size = data.size();
			memcpy(mem, data.data(), size);
		});
		return size;
	}
	return 0;
}

size_t TransferResource::preTransferData() {
	auto dev = _alloc->getDevice();
	auto table = _alloc->getDevice()->getTable();

	uint8_t *generalMem = nullptr;
	if (_memType->isHostVisible()) {
		void *targetMem = nullptr;
		if (table->vkMapMemory(dev->getDevice(), _memory, 0, VK_WHOLE_SIZE, 0, &targetMem) != VK_SUCCESS) {
			log::vtext("Vk-Error", "Fail to map internal memory: ", _resource->getName());
			return maxOf<size_t>();
		}
		generalMem = (uint8_t *)targetMem;
	}

	size_t alignment = std::max(VkDeviceSize(0x10), _alloc->getNonCoherentAtomSize());
	size_t stagingSize = 0;

	for (auto &it : _images) {
		if (it.dedicated && _alloc->getType(it.dedicatedMemType)->isHostVisible() && it.info.tiling != VK_IMAGE_TILING_OPTIMAL) {
			void *targetMem = nullptr;
			if (table->vkMapMemory(dev->getDevice(), it.dedicated, 0, VK_WHOLE_SIZE, 0, &targetMem) != VK_SUCCESS) {
				log::vtext("Vk-Error", "Fail to map dedicated memory: ", _resource->getName());
				return maxOf<size_t>();
			}
			writeData((uint8_t *)targetMem, it);
			table->vkUnmapMemory(dev->getDevice(), it.dedicated);
			if (!_alloc->getType(it.dedicatedMemType)->isHostCoherent()) {
				VkMappedMemoryRange range;
				range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				range.pNext = nullptr;
				range.memory = it.dedicated;
				range.offset = 0;
				range.size = VK_WHOLE_SIZE;
				table->vkFlushMappedMemoryRanges(dev->getDevice(), 1, &range);
			}
		} else if (it.info.tiling == VK_IMAGE_TILING_OPTIMAL || it.dedicated || generalMem == nullptr) {
			it.useStaging = true;
			stagingSize = math::align<VkDeviceSize>(stagingSize, alignment);
			it.stagingOffset = stagingSize;
			stagingSize += getFormatBlockSize(it.info.format) * it.info.extent.width * it.info.extent.height * it.info.extent.depth;
		} else {
			writeData(generalMem + it.offset, it);
		}
	}

	for (auto &it : _buffers) {
		if (it.dedicated && _alloc->getType(it.dedicatedMemType)->isHostVisible()) {
			void *targetMem = nullptr;
			if (table->vkMapMemory(dev->getDevice(), it.dedicated, 0, VK_WHOLE_SIZE, 0, &targetMem) != VK_SUCCESS) {
				log::vtext("Vk-Error", "Fail to map dedicated memory: ", _resource->getName());
				return maxOf<size_t>();
			}
			writeData((uint8_t *)targetMem, it);
			table->vkUnmapMemory(dev->getDevice(), it.dedicated);
			if (!_alloc->getType(it.dedicatedMemType)->isHostCoherent()) {
				VkMappedMemoryRange range;
				range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				range.pNext = nullptr;
				range.memory = it.dedicated;
				range.offset = 0;
				range.size = VK_WHOLE_SIZE;
				table->vkFlushMappedMemoryRanges(dev->getDevice(), 1, &range);
			}
		} else if (generalMem == nullptr || it.dedicated) {
			it.useStaging = true;
			stagingSize = math::align<VkDeviceSize>(stagingSize, alignment);
			it.stagingOffset = stagingSize;
			stagingSize += it.data->size;
		} else {
			writeData(generalMem + it.offset, it);
		}
	}

	if (generalMem) {
		table->vkUnmapMemory(dev->getDevice(), _memory);
		if (!_memType->isHostCoherent()) {
			VkMappedMemoryRange range;
			range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			range.pNext = nullptr;
			range.memory = _memory;
			range.offset = 0;
			range.size = VK_WHOLE_SIZE;
			table->vkFlushMappedMemoryRanges(dev->getDevice(), 1, &range);
		}
		generalMem = nullptr;
	}

	return stagingSize;
}

bool TransferResource::createStagingBuffer(StagingBuffer &buffer, size_t stagingSize) const {
	auto dev = _alloc->getDevice();
	auto table = _alloc->getDevice()->getTable();

	buffer.buffer.info.flags = 0;
	buffer.buffer.info.size = stagingSize;
	buffer.buffer.info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer.buffer.info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (table->vkCreateBuffer(dev->getDevice(), &buffer.buffer.info, nullptr, &buffer.buffer.buffer) != VK_SUCCESS) {
		log::vtext("Vk-Error", "Fail to create staging buffer for static resource: ", _resource->getName());
		return false;
	}

	auto mask = _alloc->getInitialTypeMask();
	buffer.buffer.req = _alloc->getBufferMemoryRequirements(buffer.buffer.buffer);

	mask &= buffer.buffer.req.requirements.memoryTypeBits;

	if (mask == 0) {
		log::vtext("Vk-Error", "Fail to find staging memory mask for static resource: ", _resource->getName());
		return false;
	}

	auto type = _alloc->findMemoryType(mask, AllocationUsage::HostTransitionSource);

	if (!type) {
		log::vtext("Vk-Error", "Fail to find staging memory type for static resource: ", _resource->getName());
		return false;
	}

	buffer.memoryTypeIndex = type->idx;

	if (_alloc->hasDedicatedFeature()) {
		VkMemoryDedicatedAllocateInfo dedicatedInfo;
		dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
		dedicatedInfo.pNext = nullptr;
		dedicatedInfo.image = VK_NULL_HANDLE;
		dedicatedInfo.buffer = buffer.buffer.buffer;

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = &dedicatedInfo;
		allocInfo.allocationSize = buffer.buffer.req.requirements.size;
		allocInfo.memoryTypeIndex = buffer.memoryTypeIndex;

		if (table->vkAllocateMemory(dev->getDevice(), &allocInfo, nullptr, &buffer.buffer.dedicated) != VK_SUCCESS) {
			log::vtext("Vk-Error", "Fail to allocate staging memory for static resource: ", _resource->getName());
			return false;
		}

		table->vkBindBufferMemory(dev->getDevice(), buffer.buffer.buffer, buffer.buffer.dedicated, 0);
	} else {
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.allocationSize = buffer.buffer.req.requirements.size;
		allocInfo.memoryTypeIndex = buffer.memoryTypeIndex;

		if (table->vkAllocateMemory(dev->getDevice(), &allocInfo, nullptr, &buffer.buffer.dedicated) != VK_SUCCESS) {
			log::vtext("Vk-Error", "Fail to allocate staging memory for static resource: ", _resource->getName());
			return false;
		}

		table->vkBindBufferMemory(dev->getDevice(), buffer.buffer.buffer, buffer.buffer.dedicated, 0);
	}

	return true;
}

bool TransferResource::writeStaging(StagingBuffer &buffer) {
	auto dev = _alloc->getDevice();
	auto table = _alloc->getDevice()->getTable();

	uint8_t *stagingMem = nullptr;
	do {
		void *targetMem = nullptr;
		if (table->vkMapMemory(dev->getDevice(), buffer.buffer.dedicated, 0, VK_WHOLE_SIZE, 0, &targetMem) != VK_SUCCESS) {
			return false;
		}
		stagingMem = (uint8_t *)targetMem;
	} while (0);

	if (!stagingMem) {
		log::vtext("Vk-Error", "Fail to map staging memory for static resource: ", _resource->getName());
		return false;
	}

	for (auto &it : _images) {
		if (it.useStaging) {
			auto size = writeData(stagingMem + it.stagingOffset, it);
			buffer.copyData.emplace_back(StagingCopy({it.stagingOffset, size, &it, nullptr}));
		}
	}

	for (auto &it : _buffers) {
		if (it.useStaging) {
			auto size = writeData(stagingMem + it.stagingOffset, it);
			buffer.copyData.emplace_back(StagingCopy({it.stagingOffset, size, nullptr, &it}));
		}
	}

	table->vkUnmapMemory(dev->getDevice(), buffer.buffer.dedicated);
	if (!_alloc->getType(buffer.memoryTypeIndex)->isHostCoherent()) {
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = nullptr;
		range.memory = _memory;
		range.offset = 0;
		range.size = VK_WHOLE_SIZE;
		table->vkFlushMappedMemoryRanges(dev->getDevice(), 1, &range);
	}

	return true;
}


TransferAttachment::~TransferAttachment() { }

auto TransferAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<TransferAttachmentHandle>::create(this, handle);
}

TransferAttachmentHandle::~TransferAttachmentHandle() { }

bool TransferAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&) {
	return true;
}

void TransferAttachmentHandle::submitInput(FrameQueue &q, Rc<gl::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	_resource = data.cast<TransferResource>();
	if (!_resource || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies, [this, cb = move(cb)] (FrameHandle &handle, bool success) {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		handle.performInQueue([this] (FrameHandle &frame) -> bool {
			if (_resource->initialize()) {
				return true;
			}
			return false;
		}, [cb = move(cb)] (FrameHandle &frame, bool success) {
			cb(success);
		}, nullptr, "TransferAttachmentHandle::submitInput");
	});
}


TransferRenderPass::~TransferRenderPass() { }

bool TransferRenderPass::init(StringView name) {
	return QueuePass::init(name, PassType::Transfer, RenderOrdering(renderqueue::RenderOrderingHighest.get() - 1), 1);
}

auto TransferRenderPass::makeFrameHandle(const FrameQueue &handle) -> Rc<PassHandle> {
	return Rc<TransferRenderPassHandle>::create(*this, handle);
}

void TransferRenderPass::prepare(gl::Device &dev) {
	QueuePass::prepare(dev);
	for (auto &it : _data->passDescriptors) {
		if (auto a = dynamic_cast<TransferAttachment *>(it->getAttachment())) {
			_attachment = a;
		}
	}
}

TransferRenderPassHandle::~TransferRenderPassHandle() { }

Vector<const CommandBuffer *> TransferRenderPassHandle::doPrepareCommands(FrameHandle &) {
	auto pass = (TransferRenderPass *)_renderPass.get();
	TransferAttachmentHandle *transfer = nullptr;
	for (auto &it : _queueData->attachments) {
		if (it.first->getAttachment() == pass->getAttachment()) {
			transfer = (TransferAttachmentHandle *)it.second->handle.get();
		}
	}

	if (!transfer) {
		return Vector<const CommandBuffer *>();
	}

	auto table = _device->getTable();
	auto buf = _pool->recordBuffer(*_device, [&] (CommandBuffer &buf) {
		Vector<VkImageMemoryBarrier> outputImageBarriers;
		Vector<VkBufferMemoryBarrier> outputBufferBarriers;

		if (!transfer->getResource()->prepareCommands(_pool->getFamilyIdx(), buf.getBuffer(), outputImageBarriers, outputBufferBarriers)) {
			return false;
		}

		VkPipelineStageFlags targetMask = 0;
		if ((_pool->getClass() & QueueOperations::Graphics) != QueueOperations::None) {
			targetMask |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		if ((_pool->getClass() & QueueOperations::Compute) != QueueOperations::None) {
			targetMask |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
		if (targetMask == 0) {
			targetMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		}

		table->vkCmdPipelineBarrier(buf.getBuffer(), VK_PIPELINE_STAGE_TRANSFER_BIT, targetMask, 0,
			0, nullptr,
			outputBufferBarriers.size(), outputBufferBarriers.data(),
			outputImageBarriers.size(), outputImageBarriers.data());
		return true;
	});

	return Vector<const CommandBuffer *>{buf};
}

void TransferRenderPassHandle::doComplete(FrameQueue &queue, Function<void(bool)> &&func, bool success) {
	if (success) {
		auto pass = (TransferRenderPass *)_renderPass.get();
		TransferAttachmentHandle *transfer = nullptr;
		for (auto &it : _queueData->attachments) {
			if (it.first->getAttachment() == pass->getAttachment()) {
				transfer = (TransferAttachmentHandle *)it.second->handle.get();
			}
		}
		transfer->getResource()->compile();
	}

	QueuePassHandle::doComplete(queue, move(func), success);
}

}
