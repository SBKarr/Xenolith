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

	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &b;
	layoutInfo.flags = 0; // TODO - use variable size descriptor array

	if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &_layout) != VK_SUCCESS) {
		return false;
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

VkImageMemoryBarrier TextureSetLayout::writeDefaults(Device &dev, VkCommandBuffer buf) {
	_defaultInit = true;

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

	return VkImageMemoryBarrier({
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		_defaultImage->getImage(), range
	});
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
	allocInfo.pNext = nullptr; // TODO - deal with VkDescriptorSetVariableDescriptorCountAllocateInfo
	allocInfo.descriptorPool = _pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	if (dev.getTable()->vkAllocateDescriptorSets(dev.getDevice(), &allocInfo, &_set) != VK_SUCCESS) {
		dev.getTable()->vkDestroyDescriptorPool(dev.getDevice(), _pool, nullptr);
		return false;
	}

	_layout = &layout;
	return gl::Object::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyDescriptorPool(d->getDevice(), (VkDescriptorPool)ptr, nullptr);
	}, gl::ObjectType::DescriptorPool, _pool);
}

void TextureSet::write(const gl::MaterialLayout &set) {
	auto table = ((Device *)_device)->getTable();
	auto dev = ((Device *)_device)->getDevice();

	// TODO - optimize this with written image view index tracking

	Vector<VkDescriptorImageInfo> images;

	for (uint32_t i = 0; i < set.usedSlots; ++ i) {
		if (set.slots[i].image) {
			images.emplace_back(VkDescriptorImageInfo({
				VK_NULL_HANDLE, ((ImageView *)set.slots[i].image.get())->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}));
		} else {
			images.emplace_back(VkDescriptorImageInfo({
				VK_NULL_HANDLE, _layout->getDefaultImageView()->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}));
		}
	}

	for (uint32_t i = set.usedSlots; i < _count; ++ i) {
		images.emplace_back(VkDescriptorImageInfo({
			VK_NULL_HANDLE, _layout->getDefaultImageView()->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		}));
	}

	Vector<VkWriteDescriptorSet> writes;
	writes.emplace_back(VkWriteDescriptorSet({
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
		0, // set
		0, // descriptor
		0, // index
		static_cast<uint32_t>(images.size()), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, images.data(),
		VK_NULL_HANDLE, VK_NULL_HANDLE
	}));

	table->vkUpdateDescriptorSets(dev, writes.size(), writes.data(), 0, nullptr);
}

}
