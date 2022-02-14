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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKMATERIALCOMPILATIONATTACHMENT_H_
#define XENOLITH_GL_VK_RENDERER_XLVKMATERIALCOMPILATIONATTACHMENT_H_

#include "XLVkRenderPass.h"
#include "XLVkAllocator.h"
#include "XLGlAttachment.h"
#include "XLGlMaterial.h"

namespace stappler::xenolith::vk {

class MaterialCompilationAttachment;

class MaterialCompiler : public gl::RenderQueue {
public:
	virtual ~MaterialCompiler();

	bool init();

	bool inProgress(const gl::MaterialAttachment *) const;
	void setInProgress(const gl::MaterialAttachment *);
	void dropInProgress(const gl::MaterialAttachment *);

	bool hasRequest(const gl::MaterialAttachment *) const;
	void appendRequest(const gl::MaterialAttachment *, Rc<gl::MaterialInputData> &&);
	Rc<gl::MaterialInputData> popRequest(const gl::MaterialAttachment *);
	void clearRequests();

	Rc<gl::FrameRequest> makeRequest(Rc<gl::MaterialInputData> &&);

protected:
	struct MaterialRequest {
		Map<gl::MaterialId, Rc<gl::Material>> materials;
		Set<gl::MaterialId> dynamic;
		Set<gl::MaterialId> remove;
	};

	MaterialCompilationAttachment *_attachment = nullptr;
	Set<const gl::MaterialAttachment *> _inProgress;
	Map<const gl::MaterialAttachment *, MaterialRequest> _requests;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMATERIALCOMPILATIONATTACHMENT_H_ */
