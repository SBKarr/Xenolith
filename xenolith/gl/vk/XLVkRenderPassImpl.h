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

#ifndef XENOLITH_GL_VK_XLVKRENDERPASSIMPL_H_
#define XENOLITH_GL_VK_XLVKRENDERPASSIMPL_H_

#include "XLVk.h"

namespace stappler::xenolith::vk {

class Device;

class RenderPassImpl : public gl::RenderPassImpl {
public:
	struct PassData {
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkRenderPass renderPassAlternative = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		Vector<VkDescriptorSetLayout> layouts;
		Vector<VkDescriptorSet> sets;

		bool cleanup(Device &dev);
	};

	virtual bool init(Device &dev, gl::RenderPassData &);

	VkRenderPass getRenderPass(bool alt = false) const;
	VkPipelineLayout getPipelineLayout() const { return _data->layout; }
	const Vector<VkDescriptorSet> &getDescriptorSets() const { return _data->sets; }

	VkDescriptorSet getDescriptorSet(uint32_t) const;

	// if async is true - update descriptors with updateAfterBind flag
	// 			   false - without updateAfterBindFlag
	virtual bool writeDescriptors(const RenderPassHandle &, bool async) const;

	virtual void perform(const RenderPassHandle &, VkCommandBuffer buf, const Callback<void()> &);

protected:
	bool initGraphicsPass(Device &dev, gl::RenderPassData &);
	bool initComputePass(Device &dev, gl::RenderPassData &);
	bool initTransferPass(Device &dev, gl::RenderPassData &);
	bool initGenericPass(Device &dev, gl::RenderPassData &);

	bool initDescriptors(Device &dev, gl::RenderPassData &, PassData &);

	Vector<VkAttachmentDescription> _attachmentDescriptions;
	Vector<VkAttachmentDescription> _attachmentDescriptionsAlternative;
	Vector<VkAttachmentReference> _attachmentReferences;
	Vector<uint32_t> _preservedAttachments;
	Vector<VkSubpassDependency> _subpassDependencies;
	Vector<VkSubpassDescription> _subpasses;
	Set<const gl::Attachment *> _variableAttachments;
	PassData *_data = nullptr;

	Vector<VkClearValue> _clearValues;
};

}

#endif /* XENOLITH_GL_VK_XLVKRENDERPASSIMPL_H_ */
