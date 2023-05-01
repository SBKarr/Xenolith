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

#ifndef XENOLITH_GL_VK_3D_XLVKRENDER3DPASS_H_
#define XENOLITH_GL_VK_3D_XLVKRENDER3DPASS_H_

#include "XLVkQueuePass.h"
#include "XLVkAttachment.h"

namespace stappler::xenolith::vk {

class Render3dVertexAttachment;
class Render3dVertexAttachmentHandle;

class Render3dPass : public QueuePass {
public:
	using Builder = renderqueue::Queue::Builder;

	struct PassCreateInfo {
		Application *app = nullptr;
		Extent2 extent;

		vk::ImageAttachment *outputAttachment = nullptr;
	};

	virtual ~Render3dPass() { }

	static void makeDefaultPass(Builder &builder, const PassCreateInfo &info);

	virtual bool init(StringView, RenderOrdering) override;

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

	const Render3dVertexAttachment *getVertexes() const { return _vertexes; }
	const MaterialAttachment *getMaterials() const { return _materials; }

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	const Render3dVertexAttachment *_vertexes = nullptr;
	const MaterialAttachment *_materials = nullptr;
};

class Render3dPassHandle : public QueuePassHandle {
public:
	virtual ~Render3dPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	virtual void prepareRenderPass(CommandBuffer &);
	virtual void writeCommands(gl::MaterialSet * materials, CommandBuffer &);
	virtual void finalizeRenderPass(CommandBuffer &);

	const Render3dVertexAttachmentHandle *_vertexes = nullptr;
	const MaterialAttachmentHandle *_materials = nullptr;
};

}

#endif /* XENOLITH_GL_VK_3D_XLVKRENDER3DPASS_H_ */
