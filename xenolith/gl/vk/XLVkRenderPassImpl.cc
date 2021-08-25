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

#include "XLVkDevice.h"
#include "XLVkRenderPassImpl.h"

namespace stappler::xenolith::vk {

bool RenderPassImpl::init(Device &dev, gl::RenderPassData &data) {
	size_t attachmentReferences = 0;
	for (auto &it : data.descriptors) {
		attachmentReferences += it->getRefs().size();
		it->setIndex(maxOf<uint32_t>());
		if (it->getAttachment()->getType() == gl::AttachmentType::Buffer) {
			continue;
		}

		VkAttachmentDescription attachment;

		bool mayAlias = false;
		for (auto &u : it->getRefs()) {
			if (u->getUsage() == gl::AttachmentUsage::InputOutput || u->getUsage() == gl::AttachmentUsage::InputDepthStencil) {
				mayAlias = true;
			}
		}

		auto imageDesc = (gl::ImageAttachmentDescriptor *)it;
		auto &info = imageDesc->getInfo();

		attachment.flags = (mayAlias ? VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT : 0);
		attachment.format = VkFormat(info.format);
		attachment.samples = VkSampleCountFlagBits(info.samples);
		attachment.loadOp = VkAttachmentLoadOp(imageDesc->getLoadOp());
		attachment.storeOp = VkAttachmentStoreOp(imageDesc->getStoreOp());
		attachment.stencilLoadOp = VkAttachmentLoadOp(imageDesc->getStencilLoadOp());
		attachment.stencilStoreOp = VkAttachmentStoreOp(imageDesc->getStencilStoreOp());
		attachment.initialLayout = VkImageLayout(imageDesc->getInitialLayout());
		attachment.finalLayout = VkImageLayout(imageDesc->getFinalLayout());

		it->setIndex(_attachmentDescriptions.size());
		_attachmentDescriptions.emplace_back(attachment);

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

	if (dev.getTable()->vkCreateRenderPass(dev.getDevice(), &renderPassInfo, nullptr, &_renderPass) == VK_SUCCESS) {
		return gl::RenderPassImpl::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyRenderPass(d->getDevice(), (VkRenderPass)ptr, nullptr);
		}, gl::ObjectType::RenderPass, _renderPass);
	}

	return false;
}

}
