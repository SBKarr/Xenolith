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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKMATERIALCOMPILATIONATTACHMENT_H_
#define XENOLITH_GL_VK_RENDERER_XLVKMATERIALCOMPILATIONATTACHMENT_H_

#include "XLVkRenderPass.h"
#include "XLVkAllocator.h"
#include "XLGlAttachment.h"
#include "XLGlMaterial.h"

namespace stappler::xenolith::vk {

class MaterialCompilationAttachment : public gl::GenericAttachment {
public:
	virtual ~MaterialCompilationAttachment();

	virtual Rc<gl::AttachmentHandle> makeFrameHandle(const gl::FrameHandle &) override;
};

class MaterialCompilationAttachmentHandle : public gl::AttachmentHandle {
public:
	virtual ~MaterialCompilationAttachmentHandle();

	virtual bool setup(gl::FrameHandle &handle) override;
	virtual bool submitInput(gl::FrameHandle &, Rc<gl::AttachmentInputData> &&) override;
	virtual void setOutput(const Rc<gl::MaterialSet> &);

	virtual const Rc<gl::MaterialInputData> &getInputData() const { return _inputData; }
	virtual const Rc<gl::MaterialSet> &getOriginalSet() const { return _originalSet; }
	virtual const Rc<gl::MaterialSet> &getOutputSet() const { return _outputSet; }

protected:
	Rc<gl::MaterialInputData> _inputData;
	Rc<gl::MaterialSet> _originalSet;
	Rc<gl::MaterialSet> _outputSet;
};

class MaterialCompilationRenderPass : public RenderPass {
public:
	virtual ~MaterialCompilationRenderPass();

	virtual bool init(StringView);

	bool inProgress(gl::MaterialAttachment *) const;
	void setInProgress(gl::MaterialAttachment *);
	void dropInProgress(gl::MaterialAttachment *);

	bool hasRequest(gl::MaterialAttachment *) const;
	void appendRequest(gl::MaterialAttachment *, Vector<Rc<gl::Material>> &&);
	Rc<gl::MaterialInputData> popRequest(gl::MaterialAttachment *);
	void clearRequests();

	uint64_t incrementOrder();

	virtual Rc<gl::RenderPassHandle> makeFrameHandle(gl::RenderPassData *, const gl::FrameHandle &) override;

	const MaterialCompilationAttachment *getMaterialAttachment() const {
		return _materialAttachment;
	}

protected:
	virtual void prepare(gl::Device &) override;

	uint64_t _order = 0;
	Set<gl::MaterialAttachment *> _inProgress;
	Map<Rc<gl::MaterialAttachment>, Map<uint32_t, Rc<gl::Material>>> _requests;
	const MaterialCompilationAttachment *_materialAttachment;
};

class MaterialCompilationRenderPassHandle : public RenderPassHandle {
public:
	virtual ~MaterialCompilationRenderPassHandle();

protected:
	virtual void addRequiredAttachment(const gl::Attachment *a, const Rc<gl::AttachmentHandle> &h) override;
	virtual Vector<VkCommandBuffer> doPrepareCommands(gl::FrameHandle &, uint32_t index) override;

	MaterialCompilationAttachmentHandle *_materialAttachment;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMATERIALCOMPILATIONATTACHMENT_H_ */
