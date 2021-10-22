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

#include "XLVkTextureSet.h"
#include <forward_list>

namespace stappler::xenolith::vk {

bool TextureSetLayout::init(Device &dev, uint32_t imageLimit) {
	_imageCount = imageLimit;

	VkDescriptorSetLayoutBinding b;
	b.binding = 0;
	b.descriptorCount = imageLimit;
	b.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; // TODO - should we extend this?
	b.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo { };
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &b;
	layoutInfo.flags = 0;

	if (dev.getInfo().features.deviceDescriptorIndexing.descriptorBindingPartiallyBound) {
		Vector<VkDescriptorBindingFlags> flags;
		flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT);

		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
		bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
		bindingFlags.pNext = nullptr;
		bindingFlags.bindingCount = 1;
		bindingFlags.pBindingFlags = flags.data();
		layoutInfo.pNext = &bindingFlags;

		if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &_layout) != VK_SUCCESS) {
			return false;
		}

		_partiallyBound = true;

	} else {
		if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &_layout) != VK_SUCCESS) {
			return false;
		}
	}

	// create dummy image

	_defaultImage = dev.getAllocator()->spawnPersistent(AllocationUsage::DeviceLocal,
			gl::ImageInfo(Extent2(1, 1), gl::ImageUsage::Sampled, gl::ImageFormat::R8_UNORM), false);

	_defaultImageView = Rc<ImageView>::create(dev, _defaultImage, gl::ImageViewInfo());

	return true;
}

void TextureSetLayout::invalidate(Device &dev) {
	if (_layout) {
		dev.getTable()->vkDestroyDescriptorSetLayout(dev.getDevice(), _layout, nullptr);
		_layout = VK_NULL_HANDLE;
	}
}

Rc<TextureSet> TextureSetLayout::acquireSet(Device &dev) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_sets.empty()) {
		lock.unlock();
		return Rc<TextureSet>::create(dev, *this);
	} else {
		auto v = move(_sets.back());
		_sets.pop_back();
		return v;
	}
}

void TextureSetLayout::releaseSet(Rc<TextureSet> &&set) {
	std::unique_lock<Mutex> lock(_mutex);
	_sets.emplace_back(move(set));
}

void TextureSetLayout::initDefault(Device &dev, gl::Loop &loop) {
	dev.acquireQueue(QueueOperations::Graphics, loop, [this] (gl::Loop &loop, const Rc<DeviceQueue> &queue) {
		auto device = (Device *)loop.getDevice().get();
		auto fence = device->acquireFence(0);
		auto pool = device->acquireCommandPool(QueueOperations::Graphics);

		fence->addRelease([dev = device, pool] {
			dev->releaseCommandPool(Rc<CommandPool>(pool));
		});

		loop.getQueue()->perform(Rc<thread::Task>::create([this, loop = Rc<gl::Loop>(&loop), pool, queue, fence] (const thread::Task &) -> bool {
			auto device = (Device *)loop->getDevice().get();
			auto table = device->getTable();
			auto buf = pool->allocBuffer(*device);

			VkCommandBufferBeginInfo beginInfo { };
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			beginInfo.pInheritanceInfo = nullptr;

			if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
				return false;
			}

			writeDefaults(*device, buf);

			if (table->vkEndCommandBuffer(buf) != VK_SUCCESS) {
				return false;
			}

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.pNext = nullptr;
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = nullptr;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &buf;
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;

			if (table->vkQueueSubmit(queue->getQueue(), 1, &submitInfo, fence->getFence()) == VK_SUCCESS) {
				return true;
			}
			return false;
		}, [this, loop = Rc<gl::Loop>(&loop), fence, queue] (const thread::Task &, bool success) {
			auto device = (Device *)loop->getDevice().get();
			if (queue) {
				device->releaseQueue(Rc<DeviceQueue>(queue));
			}
			if (success) {
				device->scheduleFence(*loop, Rc<Fence>(fence));
			} else {
				device->releaseFence(Rc<Fence>(fence));
			}
		}, this));
	}, [this] (gl::Loop &) { }, this);
}

void TextureSetLayout::writeDefaults(Device &dev, VkCommandBuffer buf) {
	// define clear range
	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	range.baseMipLevel = 0;
	range.levelCount = 1;

	// define input barrier/transition (Undefined -> TransferDst)
	VkImageMemoryBarrier inImageBarrier({
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		_defaultImage->getImage(), range
	});

	dev.getTable()->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr, // memory
			0, nullptr, // buffers
			1, &inImageBarrier);

	// clear image

	VkClearColorValue clearColor;
	clearColor.float32[0] = 0.0;
	clearColor.float32[1] = 0.0;
	clearColor.float32[2] = 0.0;
	clearColor.float32[3] = 0.0;

	dev.getTable()->vkCmdClearColorImage(buf, _defaultImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			&clearColor, 1, &range);

	VkImageMemoryBarrier outImageBarrier({
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		_defaultImage->getImage(), range
	});

	dev.getTable()->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr, // memory
			0, nullptr, // buffers
			1, &outImageBarrier);
}

bool TextureSet::init(Device &dev, const TextureSetLayout &layout) {
	_count = layout.getImageCount();

	VkDescriptorPoolSize poolSize;
	poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	poolSize.descriptorCount = _count;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = 0;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	if (dev.getTable()->vkCreateDescriptorPool(dev.getDevice(), &poolInfo, nullptr, &_pool) != VK_SUCCESS) {
		return false;
	}

	auto descriptorSetLayout = layout.getLayout();

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = _pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	if (dev.getTable()->vkAllocateDescriptorSets(dev.getDevice(), &allocInfo, &_set) != VK_SUCCESS) {
		dev.getTable()->vkDestroyDescriptorPool(dev.getDevice(), _pool, nullptr);
		return false;
	}

	_layout = &layout;
	_partiallyBound = layout.isPartiallyBound();
	return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyDescriptorPool(d->getDevice(), (VkDescriptorPool)ptr, nullptr);
	}, gl::ObjectType::DescriptorPool, _pool);
}

void TextureSet::write(const gl::MaterialLayout &set) {
	auto table = ((Device *)_device)->getTable();
	auto dev = ((Device *)_device)->getDevice();

	std::forward_list<Vector<VkDescriptorImageInfo>> imagesList;
	Vector<VkWriteDescriptorSet> writes;

	Vector<VkDescriptorImageInfo> *localImages = nullptr;

	VkWriteDescriptorSet writeData({
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
		_set, // set
		0, // descriptor
		0, // index
		0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr,
		VK_NULL_HANDLE, VK_NULL_HANDLE
	});

	if (_partiallyBound) {
		_layoutIndexes.resize(set.usedSlots, 0);
	} else {
		_layoutIndexes.resize(set.slots.size(), 0);
	}

	auto pushWritten = [&] {
		writeData.descriptorCount = localImages->size();
		writeData.pImageInfo = localImages->data();
		writes.emplace_back(move(writeData));
		writeData = VkWriteDescriptorSet ({
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
			_set, // set
			0, // descriptor
			0, // start from next index
			0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr,
			VK_NULL_HANDLE, VK_NULL_HANDLE
		});
		localImages = nullptr;
	};

	for (uint32_t i = 0; i < set.usedSlots; ++ i) {
		if (set.slots[i].image && _layoutIndexes[i] != set.slots[i].image->getIndex()) {
			if (!localImages) {
				localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
			}
			localImages->emplace_back(VkDescriptorImageInfo({
				VK_NULL_HANDLE, ((ImageView *)set.slots[i].image.get())->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}));
			auto image = (Image *)set.slots[i].image->getImage().get();
			if (auto b = image->getPendingBarrier()) {
				_pendingBarriers.emplace_back(*b);
				image->dropPendingBarrier();
			}
			_layoutIndexes[i] = set.slots[i].image->getIndex();
			++ writeData.descriptorCount;
		} else if (!_partiallyBound && !set.slots[i].image && _layoutIndexes[i] != _layout->getDefaultImageView()->getIndex()) {
			if (!localImages) {
				localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
			}
			localImages->emplace_back(VkDescriptorImageInfo({
				VK_NULL_HANDLE, _layout->getDefaultImageView()->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}));
			_layoutIndexes[i] = _layout->getDefaultImageView()->getIndex();
			++ writeData.descriptorCount;
		} else {
			// no need to write, push written
			if (writeData.descriptorCount > 0) {
				pushWritten();
			}
			writeData.dstArrayElement = i + 1; // start from next index
		}
	}

	if (!_partiallyBound) {
		// write empty
		for (uint32_t i = set.usedSlots; i < _count; ++ i) {
			if (_layoutIndexes[i] != _layout->getDefaultImageView()->getIndex()) {
				if (!localImages) {
					localImages = &imagesList.emplace_front(Vector<VkDescriptorImageInfo>());
				}
				localImages->emplace_back(VkDescriptorImageInfo({
					VK_NULL_HANDLE, _layout->getDefaultImageView()->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				}));
				_layoutIndexes[i] = _layout->getDefaultImageView()->getIndex();
				++ writeData.descriptorCount;
			} else {
				// no need to write, push written
				if (writeData.descriptorCount > 0) {
					pushWritten();
				}
				writeData.dstArrayElement = i + 1; // start from next index
			}
		}
	}

	if (writeData.descriptorCount > 0) {
		writeData.descriptorCount = localImages->size();
		writeData.pImageInfo = localImages->data();
		writes.emplace_back(move(writeData));
	}

	table->vkUpdateDescriptorSets(dev, writes.size(), writes.data(), 0, nullptr);
}

void TextureSet::dropPendingBarriers() {
	_pendingBarriers.clear();
}

}
