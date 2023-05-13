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

#include "XLVkDevice.h"
#include "XLVkRenderPassImpl.h"
#include "XLVkAttachment.h"
#include <forward_list>

namespace stappler::xenolith::vk {

DescriptorBinding::~DescriptorBinding() {
	data.clear();
}

DescriptorBinding::DescriptorBinding(VkDescriptorType type, uint32_t count) : type(type) {
	data.resize(count, DescriptorData{gl::ObjectHandle::zero(), nullptr});
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorBufferInfo &&info) {
	auto ret = move(data[idx].data);
	data[idx] = DescriptorData{gl::ObjectHandle(info.buffer->getBuffer()), move(info.buffer)};
	return ret;
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorImageInfo &&info) {
	auto ret = move(data[idx].data);
	data[idx] = DescriptorData{gl::ObjectHandle(info.imageView->getImageView()), move(info.imageView)};
	return ret;
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorBufferViewInfo &&info) {
	auto ret = move(data[idx].data);
	data[idx] = DescriptorData{gl::ObjectHandle(info.buffer->getBuffer()), move(info.buffer)};
	return ret;
}

bool RenderPassImpl::Data::cleanup(Device &dev) {
	if (renderPass) {
		dev.getTable()->vkDestroyRenderPass(dev.getDevice(), renderPass, nullptr);
		renderPass = VK_NULL_HANDLE;
	}

	if (renderPassAlternative) {
		dev.getTable()->vkDestroyRenderPass(dev.getDevice(), renderPassAlternative, nullptr);
		renderPassAlternative = VK_NULL_HANDLE;
	}

	for (auto &it : layouts) {
		for (VkDescriptorSetLayout &set : it.layouts) {
			dev.getTable()->vkDestroyDescriptorSetLayout(dev.getDevice(), set, nullptr);
		}

		if (it.layout) {
			dev.getTable()->vkDestroyPipelineLayout(dev.getDevice(), it.layout, nullptr);
		}

		if (it.descriptorPool) {
			dev.getTable()->vkDestroyDescriptorPool(dev.getDevice(), it.descriptorPool, nullptr);
		}
	}

	layouts.clear();

	return false;
}

bool RenderPassImpl::init(Device &dev, PassData &data) {
	_type = data.renderPass->getType();
	_name = data.key.str<Interface>();
	switch (_type) {
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
	if (alt && _data->renderPassAlternative) {
		return _data->renderPassAlternative;
	}
	return _data->renderPass;
}

bool RenderPassImpl::writeDescriptors(const QueuePassHandle &handle, uint32_t layoutIndex, bool async) const {
	auto dev = (Device *)_device;
	auto table = dev->getTable();
	auto data = handle.getData();

	std::forward_list<Vector<VkDescriptorImageInfo>> images;
	std::forward_list<Vector<VkDescriptorBufferInfo>> buffers;
	std::forward_list<Vector<VkBufferView>> views;

	Vector<VkWriteDescriptorSet> writes;

	auto writeDescriptor = [&] (DescriptorSet *set, const PipelineDescriptor &desc, uint32_t currentDescriptor, bool external) {
		auto a = handle.getAttachmentHandle(desc.attachment->attachment);
		if (!a) {
			return false;
		}

		Vector<VkDescriptorImageInfo> *localImages = nullptr;
		Vector<VkDescriptorBufferInfo> *localBuffers = nullptr;
		Vector<VkBufferView> *localViews = nullptr;

		VkWriteDescriptorSet writeData;
		writeData.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeData.pNext = nullptr;
		writeData.dstSet = set->set;
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
				case DescriptorType::Sampler:
				case DescriptorType::CombinedImageSampler:
				case DescriptorType::SampledImage:
				case DescriptorType::StorageImage:
				case DescriptorType::InputAttachment:
					if (!localImages) {
						localImages = &images.emplace_front(Vector<VkDescriptorImageInfo>());
					}
					if (localImages) {
						auto h = (ImageAttachmentHandle *)a;

						DescriptorImageInfo info(&desc, i, external);
						if (!h->writeDescriptor(handle, info)) {
							return false;
						} else {
							localImages->emplace_back(VkDescriptorImageInfo{info.sampler, info.imageView->getImageView(), info.layout});
							if (auto ref = set->bindings[currentDescriptor].write(i, move(info))) {
								handle.autorelease(ref);
							}
						}
					}
					break;
				case DescriptorType::StorageTexelBuffer:
				case DescriptorType::UniformTexelBuffer:
					if (!localViews) {
						localViews = &views.emplace_front(Vector<VkBufferView>());
					}
					if (localViews) {
						auto h = (TexelAttachmentHandle *)a;

						DescriptorBufferViewInfo info(&desc, i, external);
						if (h->writeDescriptor(handle, info)) {
							localViews->emplace_back(info.target);
							if (auto ref = set->bindings[currentDescriptor].write(i, move(info))) {
								handle.autorelease(ref);
							}
						} else {
							return false;
						}
					}
					break;
				case DescriptorType::UniformBuffer:
				case DescriptorType::StorageBuffer:
				case DescriptorType::UniformBufferDynamic:
				case DescriptorType::StorageBufferDynamic:
					if (!localBuffers) {
						localBuffers = &buffers.emplace_front(Vector<VkDescriptorBufferInfo>());
					}
					if (localBuffers) {
						auto h = (BufferAttachmentHandle *)a;

						DescriptorBufferInfo info(&desc, i, external);
						if (!h->writeDescriptor(handle, info)) {
							return false;
						} else {
							localBuffers->emplace_back(VkDescriptorBufferInfo{info.buffer->getBuffer(), info.offset, info.range});
							if (auto ref = set->bindings[currentDescriptor].write(i, move(info))) {
								handle.autorelease(ref);
							}
						}
					}
					break;
				case DescriptorType::Unknown:
				case DescriptorType::Attachment:
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
					writeData.dstSet = set->set;
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
	for (auto &descriptorSetData : data->pipelineLayouts[layoutIndex]->sets) {
		auto set = _data->layouts[layoutIndex].sets[currentSet];
		uint32_t currentDescriptor = 0;
		for (auto &it : descriptorSetData->descriptors) {
			if (it->updateAfterBind != async) {
				++ currentDescriptor;
				continue;
			}
			if (!writeDescriptor(set, *it, currentDescriptor, false)) {
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

void RenderPassImpl::perform(const QueuePassHandle &handle, CommandBuffer &buf, const Callback<void()> &cb) {
	bool useAlternative = false;
	for (auto &it : _variableAttachments) {
		if (auto aHandle = handle.getAttachmentHandle(it->attachment)) {
			if (aHandle->getQueueData()->image && !aHandle->getQueueData()->image->isSwapchainImage()) {
				useAlternative = true;
				break;
			}
		}
	}

	if (_data->renderPass) {
		buf.cmdBeginRenderPass(this, (Framebuffer *)handle.getFramebuffer().get(), VK_SUBPASS_CONTENTS_INLINE, useAlternative);

		cb();

		buf.cmdEndRenderPass();
	} else {
		cb();
	}
}

bool RenderPassImpl::initGraphicsPass(Device &dev, PassData &data) {
	bool hasAlternative = false;
	Data pass;

	size_t attachmentReferences = 0;
	for (auto &desc : data.attachments) {
		if (desc->attachment->type != renderqueue::AttachmentType::Image || desc->subpasses.empty()) {
			continue;
		}

		VkAttachmentDescription attachment;
		VkAttachmentDescription attachmentAlternative;

		bool mayAlias = false;
		for (auto &u : desc->subpasses) {
			if (u->usage == renderqueue::AttachmentUsage::InputOutput || u->usage == renderqueue::AttachmentUsage::InputDepthStencil) {
				mayAlias = true;
			}
		}

		auto imageAttachment = (renderqueue::ImageAttachment *)desc->attachment->attachment.get();
		auto &info = imageAttachment->getImageInfo();

		attachmentAlternative.flags = attachment.flags = (mayAlias ? VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT : 0);
		attachmentAlternative.format = attachment.format = VkFormat(info.format);
		attachmentAlternative.samples = attachment.samples = VkSampleCountFlagBits(info.samples);
		attachmentAlternative.loadOp = attachment.loadOp = VkAttachmentLoadOp(desc->loadOp);
		attachmentAlternative.storeOp = attachment.storeOp = VkAttachmentStoreOp(desc->storeOp);
		attachmentAlternative.stencilLoadOp = attachment.stencilLoadOp = VkAttachmentLoadOp(desc->stencilLoadOp);
		attachmentAlternative.stencilStoreOp = attachment.stencilStoreOp = VkAttachmentStoreOp(desc->stencilStoreOp);
		attachmentAlternative.initialLayout = attachment.initialLayout = VkImageLayout(desc->initialLayout);
		attachmentAlternative.finalLayout = attachment.finalLayout = VkImageLayout(desc->finalLayout);

		if (desc->finalLayout == renderqueue::AttachmentLayout::PresentSrc) {
			hasAlternative = true;
			attachmentAlternative.finalLayout = VkImageLayout(renderqueue::AttachmentLayout::TransferSrcOptimal);
			_variableAttachments.emplace(desc);
		}

		desc->index = _attachmentDescriptions.size();

		_attachmentDescriptions.emplace_back(attachment);
		_attachmentDescriptionsAlternative.emplace_back(attachmentAlternative);

		auto fmt = gl::getImagePixelFormat(imageAttachment->getImageInfo().format);
		switch (fmt) {
		case gl::PixelFormat::D:
			if (desc->loadOp == renderqueue::AttachmentLoadOp::Clear) {
				auto c = ((renderqueue::ImageAttachment *)desc->attachment->attachment.get())->getClearColor();
				VkClearValue clearValue;
				clearValue.depthStencil.depth = c.r;
				_clearValues.emplace_back(clearValue);
			} else {
				VkClearValue clearValue;
				clearValue.depthStencil.depth = 0.0f;
				_clearValues.emplace_back(clearValue);
			}
			break;
		case gl::PixelFormat::DS:
			if (desc->stencilLoadOp == renderqueue::AttachmentLoadOp::Clear
					|| desc->loadOp == renderqueue::AttachmentLoadOp::Clear) {
				auto c = imageAttachment->getClearColor();
				VkClearValue clearValue;
				clearValue.depthStencil.depth = c.r;
				clearValue.depthStencil.stencil = 0;
				_clearValues.emplace_back(clearValue);
			} else {
				VkClearValue clearValue;
				clearValue.depthStencil.depth = 0.0f;
				clearValue.depthStencil.stencil = 0;
				_clearValues.emplace_back(clearValue);
			}
			break;
		case gl::PixelFormat::S:
			if (desc->stencilLoadOp == renderqueue::AttachmentLoadOp::Clear) {
				VkClearValue clearValue;
				clearValue.depthStencil.stencil = 0;
				_clearValues.emplace_back(clearValue);
			} else {
				VkClearValue clearValue;
				clearValue.depthStencil.stencil = 0;
				_clearValues.emplace_back(clearValue);
			}
			break;
		default:
			if (desc->loadOp == renderqueue::AttachmentLoadOp::Clear) {
				auto c = imageAttachment->getClearColor();
				_clearValues.emplace_back(VkClearValue{c.r, c.g, c.b, c.a});
			} else {
				_clearValues.emplace_back(VkClearValue{0.0f, 0.0f, 0.0f, 1.0f});
			}
			break;
		}

		attachmentReferences += desc->subpasses.size();

		if (data.subpasses.size() >= 3 && desc->subpasses.size() < data.subpasses.size()) {
			uint32_t initialSubpass = desc->subpasses.front()->subpass->index;
			uint32_t finalSubpass = desc->subpasses.back()->subpass->index;

			for (size_t i = initialSubpass + 1; i < finalSubpass; ++ i) {
				bool found = false;
				for (auto &u : desc->subpasses) {
					if (u->subpass->index == i) {
						found = true;
					}
				}
				if (!found) {
					data.subpasses[i]->preserve.emplace_back(desc->index);
				}
			}
		}
	}

	_attachmentReferences.reserve(attachmentReferences);

	for (auto &it : data.subpasses) {
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		if (!it->inputImages.empty()) {
			auto off = _attachmentReferences.size();

			for (auto &iit : it->inputImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->pass->index;
					attachmentRef.layout = VkImageLayout(iit->layout);
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.inputAttachmentCount = it->inputImages.size();
			subpass.pInputAttachments = _attachmentReferences.data() + off;
		}

		if (!it->outputImages.empty()) {
			auto off = _attachmentReferences.size();

			for (auto &iit : it->outputImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->pass->index;
					attachmentRef.layout = VkImageLayout(iit->layout);
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.colorAttachmentCount = it->outputImages.size();
			subpass.pColorAttachments = _attachmentReferences.data() + off;
		}

		if (!it->resolveImages.empty()) {
			auto resolveImages = it->resolveImages;
			if (resolveImages.size() < it->outputImages.size()) {
				resolveImages.resize(it->outputImages.size(), nullptr);
			}

			auto off = _attachmentReferences.size();

			for (auto &iit : resolveImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->pass->index;
					attachmentRef.layout = VkImageLayout(iit->layout);
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.pResolveAttachments = _attachmentReferences.data() + off;
		}

		if (it->depthStencil) {
			VkAttachmentReference attachmentRef{};
			attachmentRef.attachment = it->depthStencil->pass->index;
			attachmentRef.layout = VkImageLayout(it->depthStencil->layout);
			_attachmentReferences.emplace_back(attachmentRef);
			subpass.pDepthStencilAttachment = &_attachmentReferences.back();
		}

		if (!it->preserve.empty()) {
			subpass.preserveAttachmentCount = it->preserve.size();
			subpass.pPreserveAttachments = it->preserve.data();
		}

		_subpasses.emplace_back(subpass);
	}

	_subpassDependencies.reserve(data.dependencies.size());

	//  TODO: deal with internal dependencies through AttachmentDependencyInfo

	for (auto &it : data.dependencies) {
		VkSubpassDependency dependency{};
		dependency.srcSubpass = (it.srcSubpass == renderqueue::SubpassDependency::External) ? VK_SUBPASS_EXTERNAL : it.srcSubpass;
		dependency.dstSubpass = (it.dstSubpass == renderqueue::SubpassDependency::External) ? VK_SUBPASS_EXTERNAL : it.dstSubpass;
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
		auto l = new Data(move(pass));
		_data = l;
		return gl::RenderPass::init(dev, [] (gl::Device *dev, gl::ObjectType, ObjectHandle ptr) {
			auto d = ((Device *)dev);
			auto l = (Data *)ptr.get();
			l->cleanup(*d);
			delete l;
		}, gl::ObjectType::RenderPass, ObjectHandle(ObjectHandle::Type(uintptr_t(l))));
	}

	return pass.cleanup(dev);
}

bool RenderPassImpl::initComputePass(Device &dev, PassData &data) {
	Data pass;
	if (initDescriptors(dev, data, pass)) {
		auto l = new Data(move(pass));
		_data = l;
		return gl::RenderPass::init(dev, [] (gl::Device *dev, gl::ObjectType, ObjectHandle ptr) {
			auto d = ((Device *)dev);
			auto l = (Data *)ptr.get();
			l->cleanup(*d);
			delete l;
		}, gl::ObjectType::RenderPass, ObjectHandle(ObjectHandle::Type(uintptr_t(l))));
	}

	return pass.cleanup(dev);
}

bool RenderPassImpl::initTransferPass(Device &dev, PassData &) {
	// init nothing - no descriptors or render pass implementation needed
	auto l = new Data();
	_data = l;
	return gl::RenderPass::init(dev, [] (gl::Device *dev, gl::ObjectType, ObjectHandle ptr) {
		auto d = ((Device *)dev);
		auto l = (Data *)ptr.get();
		l->cleanup(*d);
		delete l;
	}, gl::ObjectType::RenderPass, ObjectHandle(ObjectHandle::Type(uintptr_t(l))));
}

bool RenderPassImpl::initGenericPass(Device &dev, PassData &) {
	// init nothing - no descriptors or render pass implementation needed
	auto l = new Data();
	_data = l;
	return gl::RenderPass::init(dev, [] (gl::Device *dev, gl::ObjectType, ObjectHandle ptr) {
		auto d = ((Device *)dev);
		auto l = (Data *)ptr.get();
		l->cleanup(*d);
		delete l;
	}, gl::ObjectType::RenderPass, ObjectHandle(ObjectHandle::Type(uintptr_t(l))));
}

bool RenderPassImpl::initDescriptors(Device &dev, const PassData &data, Data &pass) {
	auto cleanupLayouts = [&] {
		for (auto &it : pass.layouts) {
			for (VkDescriptorSetLayout &set : it.layouts) {
				dev.getTable()->vkDestroyDescriptorSetLayout(dev.getDevice(), set, nullptr);
			}

			if (it.layout) {
				dev.getTable()->vkDestroyPipelineLayout(dev.getDevice(), it.layout, nullptr);
			}

			if (it.descriptorPool) {
				dev.getTable()->vkDestroyDescriptorPool(dev.getDevice(), it.descriptorPool, nullptr);
			}
		}
		pass.layouts.clear();
	};

	for (auto &it : data.pipelineLayouts) {
		LayoutData &layoutData = pass.layouts.emplace_back();
		if (!initDescriptors(dev, *it, pass, layoutData)) {
			cleanupLayouts();
			return false;
		}
	}

	return true;
}

bool RenderPassImpl::initDescriptors(Device &dev, const renderqueue::PipelineLayoutData &data, Data &, LayoutData &layoutData) {
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
	for (auto &setData : data.sets) {
		++ maxSets;

		auto descriptorSet = Rc<DescriptorSet>::alloc();

		bool hasFlags = false;
		Vector<VkDescriptorBindingFlags> flags;
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		Vector<VkDescriptorSetLayoutBinding> bindings; bindings.reserve(setData->descriptors.size());
		size_t bindingIdx = 0;
		for (auto &binding : setData->descriptors) {
			if (binding->updateAfterBind) {
				flags.emplace_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
				hasFlags = true;
				updateAfterBind = true;
			} else {
				flags.emplace_back(0);
			}

			binding->index = bindingIdx;

			VkDescriptorSetLayoutBinding b;
			b.binding = bindingIdx;
			b.descriptorCount = binding->count;
			b.descriptorType = VkDescriptorType(binding->type);
			b.stageFlags = VkShaderStageFlags(binding->stages);
			if (binding->type == DescriptorType::Sampler) {
				// do nothing
				log::vtext("vk::RenderPassImpl", "gl::DescriptorType::Sampler is not supported for descriptors");
			} else {
				incrementSize(VkDescriptorType(binding->type), binding->count);
				b.pImmutableSamplers = nullptr;
			}
			bindings.emplace_back(b);
			descriptorSet->bindings.emplace_back(DescriptorBinding(VkDescriptorType(binding->type), binding->count));
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
				layoutData.sets.emplace_back(move(descriptorSet));
				layoutData.layouts.emplace_back(setLayout);
			} else {
				return false;
			}
		} else {
			if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr, &setLayout) == VK_SUCCESS) {
				layoutData.sets.emplace_back(move(descriptorSet));
				layoutData.layouts.emplace_back(setLayout);
			} else {
				return false;
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

	if (dev.getTable()->vkCreateDescriptorPool(dev.getDevice(), &poolInfo, nullptr, &layoutData.descriptorPool) != VK_SUCCESS) {
		return false;
	}

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = layoutData.descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layoutData.layouts.size());
	allocInfo.pSetLayouts = layoutData.layouts.data();

	Vector<VkDescriptorSet> sets; sets.resize(layoutData.layouts.size());

	if (dev.getTable()->vkAllocateDescriptorSets(dev.getDevice(), &allocInfo, sets.data()) != VK_SUCCESS) {
		layoutData.sets.clear();
		return false;
	}

	for (size_t i = 0; i < sets.size(); ++ i) {
		layoutData.sets[i]->set = sets[i];
	}

	Vector<VkPushConstantRange> ranges;

	auto addRange = [&] (VkShaderStageFlags flags, uint32_t offset, uint32_t size) {
		for (auto &it : ranges) {
			if (it.stageFlags == flags) {
				if (offset < it.offset) {
					it.size += (it.offset - offset);
					it.offset = offset;
				}
				if (size > it.size) {
					it.size = size;
				}
				return;
			}
		}

		ranges.emplace_back(VkPushConstantRange{flags, offset, size});
	};

	for (auto &pipeline : data.graphicPipelines) {
		for (auto &shader : pipeline->shaders) {
			for (auto &constantBlock : shader.data->constants) {
				addRange(VkShaderStageFlags(shader.data->stage), constantBlock.offset, constantBlock.size);
			}
		}
	}

	for (auto &pipeline : data.computePipelines) {
		for (auto &constantBlock : pipeline->shader.data->constants) {
			addRange(VkShaderStageFlags(pipeline->shader.data->stage), constantBlock.offset, constantBlock.size);
		}
	}

	auto textureSetLayout = dev.getTextureSetLayout();
	Vector<VkDescriptorSetLayout> layouts(layoutData.layouts);
	if (data.usesTextureSet) {
		layouts.emplace_back(textureSetLayout->getLayout());
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = 0;
	pipelineLayoutInfo.setLayoutCount = layouts.size();
	pipelineLayoutInfo.pSetLayouts = layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = ranges.size();
	pipelineLayoutInfo.pPushConstantRanges = ranges.data();

	if (dev.getTable()->vkCreatePipelineLayout(dev.getDevice(), &pipelineLayoutInfo, nullptr, &layoutData.layout) == VK_SUCCESS) {
		return true;
	}

	return false;
}

}
