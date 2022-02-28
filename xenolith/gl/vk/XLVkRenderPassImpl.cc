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

#include "XLVkDevice.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkAttachment.h"
#include <forward_list>

namespace stappler::xenolith::vk {

bool RenderPassImpl::PassData::cleanup(Device &dev) {
	for (VkDescriptorSetLayout &it : layouts) {
		dev.getTable()->vkDestroyDescriptorSetLayout(dev.getDevice(), it, nullptr);
	}

	if (renderPass) {
		dev.getTable()->vkDestroyRenderPass(dev.getDevice(), renderPass, nullptr);
	}

	if (renderPassAlternative) {
		dev.getTable()->vkDestroyRenderPass(dev.getDevice(), renderPassAlternative, nullptr);
	}

	if (layout) {
		dev.getTable()->vkDestroyPipelineLayout(dev.getDevice(), layout, nullptr);
	}

	if (descriptorPool) {
		dev.getTable()->vkDestroyDescriptorPool(dev.getDevice(), descriptorPool, nullptr);
	}

	return false;
}

bool RenderPassImpl::init(Device &dev, gl::RenderPassData &data) {
	switch (data.renderPass->getType()) {
	case gl::RenderPassType::Graphics:
		return initGraphicsPass(dev, data);
		break;
	case gl::RenderPassType::Compute:
		return initComputePass(dev, data);
		break;
	case gl::RenderPassType::Transfer:
		return initTransferPass(dev, data);
		break;
	case gl::RenderPassType::Generic:
		return initGenericPass(dev, data);
		break;
	}

	return false;
}

VkRenderPass RenderPassImpl::getRenderPass(bool alt) const {
	if (!alt) {
		return _data->renderPass;
	} else {
		return _data->renderPassAlternative;
	}
}

VkDescriptorSet RenderPassImpl::getDescriptorSet(uint32_t idx) const {
	if (idx >= _data->sets.size()) {
		return VK_NULL_HANDLE;
	}
	return _data->sets[idx];
}

bool RenderPassImpl::writeDescriptors(const RenderPassHandle &handle, bool async) const {
	auto dev = (Device *)_device;
	auto table = dev->getTable();
	auto data = handle.getData();

	std::forward_list<Vector<VkDescriptorImageInfo>> images;
	std::forward_list<Vector<VkDescriptorBufferInfo>> buffers;
	std::forward_list<Vector<VkBufferView>> views;

	Vector<VkWriteDescriptorSet> writes;

	auto writeDescriptor = [&] (VkDescriptorSet set, const gl::PipelineDescriptor &desc, uint32_t currentDescriptor, bool external) {
		auto a = handle.getAttachmentHandle(desc.attachment);
		if (!a) {
			return false;
		}

		Vector<VkDescriptorImageInfo> *localImages = nullptr;
		Vector<VkDescriptorBufferInfo> *localBuffers = nullptr;
		Vector<VkBufferView> *localViews = nullptr;

		VkWriteDescriptorSet writeData;
		writeData.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeData.pNext = nullptr;
		writeData.dstSet = set;
		writeData.dstBinding = currentDescriptor;
		writeData.dstArrayElement = 0;
		writeData.descriptorCount = 0;
		writeData.descriptorType = VkDescriptorType(desc.type);
		writeData.pImageInfo = VK_NULL_HANDLE;
		writeData.pBufferInfo = VK_NULL_HANDLE;
		writeData.pTexelBufferView = VK_NULL_HANDLE;

		auto c = a->getDescriptorArraySize(handle, desc, external);
		for (uint32_t i = 0; i < c; ++ i) {
			if (a->isDescriptorDirty(handle, desc, i, external)) {
				switch (desc.type) {
				case gl::DescriptorType::Sampler:
				case gl::DescriptorType::CombinedImageSampler:
				case gl::DescriptorType::SampledImage:
				case gl::DescriptorType::StorageImage:
				case gl::DescriptorType::InputAttachment:
					if (!localImages) {
						localImages = &images.emplace_front(Vector<VkDescriptorImageInfo>());
					}
					if (localImages) {
						auto &dst = localImages->emplace_back(VkDescriptorImageInfo());
						auto h = (ImageAttachmentHandle *)a;
						if (!h->writeDescriptor(handle, desc, i, external, dst)) {
							return false;
						}
					}
					break;
				case gl::DescriptorType::StorageTexelBuffer:
				case gl::DescriptorType::UniformTexelBuffer:
					if (!localViews) {
						localViews = &views.emplace_front(Vector<VkBufferView>());
					}
					if (localViews) {
						auto h = (TexelAttachmentHandle *)a;
						if (auto v = h->getDescriptor(handle, desc, i, external)) {
							localViews->emplace_back(v);
						} else {
							return false;
						}
					}
					break;
				case gl::DescriptorType::UniformBuffer:
				case gl::DescriptorType::StorageBuffer:
				case gl::DescriptorType::UniformBufferDynamic:
				case gl::DescriptorType::StorageBufferDynamic:
					if (!localBuffers) {
						localBuffers = &buffers.emplace_front(Vector<VkDescriptorBufferInfo>());
					}
					if (localBuffers) {
						auto &dst = localBuffers->emplace_back(VkDescriptorBufferInfo());
						auto h = (BufferAttachmentHandle *)a;
						if (!h->writeDescriptor(handle, desc, i, external, dst)) {
							return false;
						}
					}
					break;
				case gl::DescriptorType::Unknown:
					break;
				}
				++ writeData.descriptorCount;
			} else {
				if (writeData.descriptorCount > 0) {
					if (localImages) {
						writeData.pImageInfo = localImages->data();
					}
					if (localBuffers) {
						writeData.pBufferInfo = localBuffers->data();
					}
					if (localViews) {
						writeData.pTexelBufferView = localViews->data();
					}

					writes.emplace_back(move(writeData));

					writeData.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writeData.pNext = nullptr;
					writeData.dstSet = set;
					writeData.dstBinding = currentDescriptor;
					writeData.descriptorCount = 0;
					writeData.descriptorType = VkDescriptorType(desc.type);
					writeData.pImageInfo = VK_NULL_HANDLE;
					writeData.pBufferInfo = VK_NULL_HANDLE;
					writeData.pTexelBufferView = VK_NULL_HANDLE;

					localImages = nullptr;
					localBuffers = nullptr;
					localViews = nullptr;
				}

				writeData.dstArrayElement = i + 1;
			}
		}

		if (writeData.descriptorCount > 0) {
			if (localImages) {
				writeData.pImageInfo = localImages->data();
			}
			if (localBuffers) {
				writeData.pBufferInfo = localBuffers->data();
			}
			if (localViews) {
				writeData.pTexelBufferView = localViews->data();
			}

			writes.emplace_back(move(writeData));
		}
		return true;
	};

	uint32_t currentSet = 0;
	if (!data->queueDescriptors.empty()) {
		auto set = getDescriptorSet(currentSet);
		uint32_t currentDescriptor = 0;
		for (auto &it : data->queueDescriptors) {
			if (it->updateAfterBind != async) {
				continue;
			}
			if (!writeDescriptor(set, *it, currentDescriptor, false)) {
				return false;
			}
			++ currentDescriptor;
		}
		++ currentSet;
	}

	if (!data->extraDescriptors.empty()) {
		auto set = getDescriptorSet(currentSet);
		uint32_t currentDescriptor = 0;
		for (auto &it : data->extraDescriptors) {
			if (it.updateAfterBind != async) {
				continue;
			}
			if (!writeDescriptor(set, it, currentDescriptor, true)) {
				return false;
			}
			++ currentDescriptor;
		}
		++ currentSet;
	}

	if (writes.empty()) {
		return true;
	}

	table->vkUpdateDescriptorSets(dev->getDevice(), writes.size(), writes.data(), 0, nullptr);
	return true;
}

void RenderPassImpl::perform(const RenderPassHandle &handle, VkCommandBuffer buf, const Callback<void()> &cb) {
	bool useAlternative = false;
	for (auto &it : _variableAttachments) {
		if (auto aHandle = handle.getAttachmentHandle(it)) {
			if (aHandle->getQueueData()->image && !aHandle->getQueueData()->image->isSwapchainImage) {
				useAlternative = true;
				break;
			}
		}
	}

	if (auto pass = getRenderPass(useAlternative)) {
		auto dev = (Device *)_device;
		auto table = dev->getTable();

		auto fb = (Framebuffer *)handle.getFramebuffer().get();
		auto currentExtent = fb->getExtent();

		VkRenderPassBeginInfo renderPassInfo { };
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = pass;
		renderPassInfo.framebuffer = fb->getFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{currentExtent.width, currentExtent.height};
		renderPassInfo.clearValueCount = _clearValues.size();
		renderPassInfo.pClearValues = _clearValues.data();
		table->vkCmdBeginRenderPass(buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		cb();

		table->vkCmdEndRenderPass(buf);
	} else {
		cb();
	}
}

bool RenderPassImpl::initGraphicsPass(Device &dev, gl::RenderPassData &data) {
	bool hasAlternative = false;
	PassData pass;

	size_t attachmentReferences = 0;
	for (auto &it : data.descriptors) {
		if (!gl::isImageAttachmentType(it->getAttachment()->getType())) {
			continue;
		}

		VkAttachmentDescription attachment;
		VkAttachmentDescription attachmentAlternative;

		bool mayAlias = false;
		for (auto &u : it->getRefs()) {
			if (u->getUsage() == gl::AttachmentUsage::InputOutput || u->getUsage() == gl::AttachmentUsage::InputDepthStencil) {
				mayAlias = true;
			}
		}

		auto imageDesc = (gl::ImageAttachmentDescriptor *)it;
		auto &info = imageDesc->getInfo();

		attachmentAlternative.flags = attachment.flags = (mayAlias ? VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT : 0);
		attachmentAlternative.format = attachment.format = VkFormat(info.format);
		attachmentAlternative.samples = attachment.samples = VkSampleCountFlagBits(info.samples);
		attachmentAlternative.loadOp = attachment.loadOp = VkAttachmentLoadOp(imageDesc->getLoadOp());
		attachmentAlternative.storeOp = attachment.storeOp = VkAttachmentStoreOp(imageDesc->getStoreOp());
		attachmentAlternative.stencilLoadOp = attachment.stencilLoadOp = VkAttachmentLoadOp(imageDesc->getStencilLoadOp());
		attachmentAlternative.stencilStoreOp = attachment.stencilStoreOp = VkAttachmentStoreOp(imageDesc->getStencilStoreOp());
		attachmentAlternative.initialLayout = attachment.initialLayout = VkImageLayout(imageDesc->getInitialLayout());
		attachmentAlternative.finalLayout = attachment.finalLayout = VkImageLayout(imageDesc->getFinalLayout());

		if (imageDesc->getFinalLayout() == gl::AttachmentLayout::PresentSrc) {
			hasAlternative = true;
			attachmentAlternative.finalLayout = VkImageLayout(gl::AttachmentLayout::TransferSrcOptimal);
			_variableAttachments.emplace(it->getAttachment());
		}

		it->setIndex(_attachmentDescriptions.size());

		_attachmentDescriptions.emplace_back(attachment);
		_attachmentDescriptionsAlternative.emplace_back(attachmentAlternative);

		if (imageDesc->getLoadOp() == gl::AttachmentLoadOp::Clear) {
			auto c = ((gl::ImageAttachment *)imageDesc->getAttachment())->getClearColor();
			_clearValues.emplace_back(VkClearValue{c.r, c.g, c.b, c.a});
		} else {
			_clearValues.emplace_back(VkClearValue{0.0f, 0.0f, 0.0f, 1.0f});
		}

		attachmentReferences += it->getRefs().size();

		if (data.subpasses.size() > 3 && it->getRefs().size() < data.subpasses.size()) {
			size_t initialSubpass = it->getRefs().front()->getSubpass();
			size_t finalSubpass = it->getRefs().back()->getSubpass();

			for (size_t i = initialSubpass + 1; i < finalSubpass; ++ i) {
				bool found = false;
				for (auto &u : it->getRefs()) {
					if (u->getSubpass() == i) {
						found = true;
					}
				}
				if (!found) {
					data.subpasses[i].preserve.emplace_back(it->getIndex());
				}
			}
		}
	}

	_attachmentReferences.reserve(attachmentReferences);

	for (auto &it : data.subpasses) {
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		if (!it.inputImages.empty()) {
			auto off = _attachmentReferences.size();

			for (auto &iit : it.inputImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->getDescriptor()->getIndex();
					attachmentRef.layout = VkImageLayout(iit->getLayout());
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.inputAttachmentCount = it.inputImages.size();
			subpass.pInputAttachments = _attachmentReferences.data() + off;
		}

		if (!it.outputImages.empty()) {
			auto off = _attachmentReferences.size();

			for (auto &iit : it.outputImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->getDescriptor()->getIndex();
					attachmentRef.layout = VkImageLayout(iit->getLayout());
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.colorAttachmentCount = it.outputImages.size();
			subpass.pColorAttachments = _attachmentReferences.data() + off;
		}

		if (!it.resolveImages.empty()) {
			if (it.resolveImages.size() < it.outputImages.size()) {
				it.resolveImages.resize(it.outputImages.size(), nullptr);
			}

			auto off = _attachmentReferences.size();

			for (auto &iit : it.resolveImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->getDescriptor()->getIndex();
					attachmentRef.layout = VkImageLayout(iit->getLayout());
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.pResolveAttachments = _attachmentReferences.data() + off;
		}

		if (it.depthStencil) {
			VkAttachmentReference attachmentRef{};
			attachmentRef.attachment = it.depthStencil->getDescriptor()->getIndex();
			attachmentRef.layout = VkImageLayout(it.depthStencil->getLayout());
			_attachmentReferences.emplace_back(attachmentRef);
			subpass.pDepthStencilAttachment = &_attachmentReferences.back();
		}

		if (!it.preserve.empty()) {
			subpass.preserveAttachmentCount = it.preserve.size();
			subpass.pPreserveAttachments = it.preserve.data();
		}

		_subpasses.emplace_back(subpass);
	}

	_subpassDependencies.reserve(data.dependencies.size());

	//  TODO: deal with internal dependencies through AttachmentDependencyInfo

	for (auto &it : data.dependencies) {
		VkSubpassDependency dependency{};
		dependency.srcSubpass = (it.srcSubpass == gl::RenderSubpassDependency::External) ? VK_SUBPASS_EXTERNAL : it.srcSubpass;
		dependency.dstSubpass = (it.dstSubpass == gl::RenderSubpassDependency::External) ? VK_SUBPASS_EXTERNAL : it.dstSubpass;
		dependency.srcStageMask = VkPipelineStageFlags(it.srcStage);
		dependency.srcAccessMask = VkAccessFlags(it.srcAccess);
		dependency.dstStageMask = VkPipelineStageFlags(it.dstStage);
		dependency.dstAccessMask = VkAccessFlags(it.dstAccess);
		dependency.dependencyFlags = 0;
		if (it.byRegion) {
			dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}

		_subpassDependencies.emplace_back(dependency);
	}

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = _attachmentDescriptions.size();
	renderPassInfo.pAttachments = _attachmentDescriptions.data();
	renderPassInfo.subpassCount = _subpasses.size();
	renderPassInfo.pSubpasses = _subpasses.data();
	renderPassInfo.dependencyCount = _subpassDependencies.size();
	renderPassInfo.pDependencies = _subpassDependencies.data();

	if (dev.getTable()->vkCreateRenderPass(dev.getDevice(), &renderPassInfo, nullptr, &pass.renderPass) != VK_SUCCESS) {
		return pass.cleanup(dev);
	}

	if (hasAlternative) {
		renderPassInfo.attachmentCount = _attachmentDescriptionsAlternative.size();
		renderPassInfo.pAttachments = _attachmentDescriptionsAlternative.data();

		if (dev.getTable()->vkCreateRenderPass(dev.getDevice(), &renderPassInfo, nullptr, &pass.renderPassAlternative) != VK_SUCCESS) {
			return pass.cleanup(dev);
		}
	}

	if (initDescriptors(dev, data, pass)) {
		auto l = new PassData(move(pass));
		_data = l;
		return gl::RenderPassImpl::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			auto l = (PassData *)ptr;
			l->cleanup(*d);
			delete l;
		}, gl::ObjectType::RenderPass, l);
	}

	return pass.cleanup(dev);
}

bool RenderPassImpl::initComputePass(Device &dev, gl::RenderPassData &) {
	return false; // TODO - deal with Compute passes
}

bool RenderPassImpl::initTransferPass(Device &dev, gl::RenderPassData &) {
	// init nothing - no descriptors or render pass implementation needed
	auto l = new PassData();
	_data = l;
	return gl::RenderPassImpl::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		auto d = ((Device *)dev);
		auto l = (PassData *)ptr;
		l->cleanup(*d);
		delete l;
	}, gl::ObjectType::RenderPass, l);
}

bool RenderPassImpl::initGenericPass(Device &dev, gl::RenderPassData &) {
	// init nothing - no descriptors or render pass implementation needed
	auto l = new PassData();
	_data = l;
	return gl::RenderPassImpl::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
		auto d = ((Device *)dev);
		auto l = (PassData *)ptr;
		l->cleanup(*d);
		delete l;
	}, gl::ObjectType::RenderPass, l);
}

bool RenderPassImpl::initDescriptors(Device &dev, gl::RenderPassData &data, PassData &pass) {
	Vector<VkDescriptorPoolSize> sizes;

	auto incrementSize = [&sizes] (VkDescriptorType type, uint32_t count) {
		auto lb = std::lower_bound(sizes.begin(), sizes.end(), type, [] (const VkDescriptorPoolSize &l, VkDescriptorType r) {
			return l.type < r;
		});
		if (lb == sizes.end()) {
			sizes.emplace_back(VkDescriptorPoolSize({type, count}));
		} else if (lb->type == type) {
			lb->descriptorCount += count;
		} else {
			sizes.emplace(lb, VkDescriptorPoolSize({type, count}));
		}
	};

	bool updateAfterBind = false;

	uint32_t maxSets = 0;
	if (!data.queueDescriptors.empty()) {
		++ maxSets;

		bool hasFlags = false;
		Vector<VkDescriptorBindingFlags> flags;
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		Vector<VkDescriptorSetLayoutBinding> bindings; bindings.reserve(data.queueDescriptors.size());
		size_t bindingIdx = 0;
		for (auto &binding : data.queueDescriptors) {
			if (binding->updateAfterBind) {
				flags.emplace_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
				hasFlags = true;
				updateAfterBind = true;
			} else {
				flags.emplace_back(0);
			}

			binding->descriptor->setIndex(bindingIdx);

			VkDescriptorSetLayoutBinding b;
			b.binding = bindingIdx;
			b.descriptorCount = binding->count;
			b.descriptorType = VkDescriptorType(binding->type);
			b.stageFlags = VkShaderStageFlags(binding->stages);
			if (binding->type == gl::DescriptorType::Sampler) {
				b.pImmutableSamplers = dev.getImmutableSamplers().data();
			} else {
				incrementSize(VkDescriptorType(binding->type), std::max(binding->count, binding->maxCount));
				b.pImmutableSamplers = nullptr;
			}
			bindings.emplace_back(b);
			++ bindingIdx;
		}
		VkDescriptorSetLayoutCreateInfo layoutInfo { };
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = nullptr;
		layoutInfo.bindingCount = bindings.size();
		layoutInfo.pBindings = bindings.data();
		layoutInfo.flags = 0;

		if (hasFlags) {
			layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

			VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
			bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
			bindingFlags.pNext = nullptr;
			bindingFlags.bindingCount = flags.size();
			bindingFlags.pBindingFlags = flags.data();
			layoutInfo.pNext = &bindingFlags;

			if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &setLayout) == VK_SUCCESS) {
				pass.layouts.emplace_back(setLayout);
			} else {
				return pass.cleanup(dev);
			}
		} else {
			if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &setLayout) == VK_SUCCESS) {
				pass.layouts.emplace_back(setLayout);
			} else {
				return pass.cleanup(dev);
			}
		}
	}

	if (!data.extraDescriptors.empty()) {
		++ maxSets;

		bool hasFlags = false;
		Vector<VkDescriptorBindingFlags> flags;
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		Vector<VkDescriptorSetLayoutBinding> bindings; bindings.reserve(data.extraDescriptors.size());
		size_t bindingIdx = 0;
		for (auto &binding : data.extraDescriptors) {
			if (binding.updateAfterBind) {
				flags.emplace_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
				hasFlags = true;
				updateAfterBind = true;
			} else {
				flags.emplace_back(0);
			}

			VkDescriptorSetLayoutBinding b;
			b.binding = bindingIdx;
			b.descriptorCount = binding.count;
			b.descriptorType = VkDescriptorType(binding.type);
			b.stageFlags = VkShaderStageFlags(binding.stages);
			if (binding.type == gl::DescriptorType::Sampler) {
				b.pImmutableSamplers = dev.getImmutableSamplers().data();
			} else {
				incrementSize(VkDescriptorType(binding.type), std::max(binding.count, binding.maxCount));
				b.pImmutableSamplers = nullptr;
			}
			bindings.emplace_back(b);
			++ bindingIdx;
		}
		VkDescriptorSetLayoutCreateInfo layoutInfo { };
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

		layoutInfo.bindingCount = bindings.size();
		layoutInfo.pBindings = bindings.data();
		layoutInfo.flags = 0;

		if (hasFlags) {
			layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

			VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
			bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
			bindingFlags.pNext = nullptr;
			bindingFlags.bindingCount = flags.size();
			bindingFlags.pBindingFlags = flags.data();
			layoutInfo.pNext = &bindingFlags;

			if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &setLayout) == VK_SUCCESS) {
				pass.layouts.emplace_back(setLayout);
			} else {
				return pass.cleanup(dev);
			}
		} else {
			if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &setLayout) == VK_SUCCESS) {
				pass.layouts.emplace_back(setLayout);
			} else {
				return pass.cleanup(dev);
			}
		}
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	if (updateAfterBind) {
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
	} else {
		poolInfo.flags = 0;
	}
	poolInfo.poolSizeCount = sizes.size();
	poolInfo.pPoolSizes = sizes.data();
	poolInfo.maxSets = maxSets;

	if (dev.getTable()->vkCreateDescriptorPool(dev.getDevice(), &poolInfo, nullptr, &pass.descriptorPool) != VK_SUCCESS) {
		return pass.cleanup(dev);
	}

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = pass.descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(pass.layouts.size());
	allocInfo.pSetLayouts = pass.layouts.data();

	pass.sets.resize(pass.layouts.size());
	if (dev.getTable()->vkAllocateDescriptorSets(dev.getDevice(), &allocInfo, pass.sets.data()) != VK_SUCCESS) {
		pass.sets.clear();
		return pass.cleanup(dev);
	}

	// allow 12 bytes for Vertex and fragment shaders
	VkPushConstantRange range = {
		VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		12
	};

	auto textureSetLayout = dev.getTextureSetLayout();
	Vector<VkDescriptorSetLayout> layouts(pass.layouts);
	for (auto &it : data.descriptors) {
		if (it->usesTextureSet()) {
			layouts.emplace_back(textureSetLayout->getLayout());
		}
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = 0;
	pipelineLayoutInfo.setLayoutCount = layouts.size();
	pipelineLayoutInfo.pSetLayouts = layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = { &range };

	if (dev.getTable()->vkCreatePipelineLayout(dev.getDevice(), &pipelineLayoutInfo, nullptr, &pass.layout) == VK_SUCCESS) {
		return true;
	}

	return pass.cleanup(dev);
}

}
