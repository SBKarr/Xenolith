/**
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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKMESHCOMPILER_H_
#define XENOLITH_GL_VK_RENDERER_XLVKMESHCOMPILER_H_

#include "XLVkQueuePass.h"
#include "XLVkAllocator.h"
#include "XLRenderQueueAttachment.h"
#include "XLGlMesh.h"

namespace stappler::xenolith::vk {

class MeshCompilerAttachment;

class MeshCompiler : public renderqueue::Queue {
public:
	virtual ~MeshCompiler();

	bool init();

	bool inProgress(const gl::MeshAttachment *) const;
	void setInProgress(const gl::MeshAttachment *);
	void dropInProgress(const gl::MeshAttachment *);

	bool hasRequest(const gl::MeshAttachment *) const;
	void appendRequest(const gl::MeshAttachment *, Rc<gl::MeshInputData> &&,
			Vector<Rc<renderqueue::DependencyEvent>> &&deps);
	void clearRequests();

	Rc<FrameRequest> makeRequest(Rc<gl::MeshInputData> &&, Vector<Rc<renderqueue::DependencyEvent>> &&deps);
	void runMeshCompilationFrame(gl::Loop &loop, Rc<gl::MeshInputData> &&req,
			Vector<Rc<renderqueue::DependencyEvent>> &&deps);

protected:
	using renderqueue::Queue::init;

	struct MeshRequest {
		Set<Rc<gl::MeshIndex>> toAdd;
		Set<Rc<gl::MeshIndex>> toRemove;
		Vector<Rc<renderqueue::DependencyEvent>> deps;
	};

	MeshCompilerAttachment *_attachment = nullptr;
	Set<const gl::MeshAttachment *> _inProgress;
	Map<const gl::MeshAttachment *, MeshRequest> _requests;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKMESHCOMPILER_H_ */
