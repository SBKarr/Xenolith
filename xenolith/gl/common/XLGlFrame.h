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

#ifndef XENOLITH_GL_COMMON_XLGLFRAME_H_
#define XENOLITH_GL_COMMON_XLGLFRAME_H_

#include "XLGlRenderQueue.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::gl {

class FrameHandle : public Ref {
public:
	virtual ~FrameHandle();

	bool init(Loop &, RenderQueue &, uint64_t order, uint32_t gen, bool readyForSubmit = false);

	void update(bool init = false);

	uint64_t getOrder() const { return _order; }
	uint64_t getGen() const { return _gen; }
	Loop *getLoop() const { return _loop; }
	Device *getDevice() const { return _device; }
	const Rc<RenderQueue> &getQueue() const { return _queue; }

	// spinners within frame should not spin directly on loop to preserve FrameHandle object
	virtual void schedule(Function<bool(FrameHandle &, Loop::Context &)> &&);

	// thread tasks within frame should not be performed directly on loop's queue to preserve FrameHandle object
	virtual void performInQueue(Function<void(FrameHandle &)> &&, Ref * = nullptr);
	virtual void performInQueue(Function<bool(FrameHandle &)> &&, Function<void(FrameHandle &, bool)> &&, Ref * = nullptr);

	// thread tasks within frame should not be performed directly on loop's queue to preserve FrameHandle object
	virtual void performOnGlThread(Function<void(FrameHandle &)> &&, Ref * = nullptr, bool immediate = true);

	virtual bool isSubmitted() const { return _submitted; }
	virtual bool isInputRequired() const;
	virtual bool isPresentable() const;
	virtual bool isValid() const;
	virtual bool isValidFlag() const { return _valid; }
	virtual bool isInputSubmitted() const { return _inputSubmitted == _inputAttachments.size(); }

	virtual const Vector<Rc<AttachmentHandle>> &getInputAttachments() const { return _inputAttachments; }
	virtual bool submitInput(const Rc<AttachmentHandle> &, Rc<AttachmentInputData> &&);

	virtual void setAttachmentReady(const Rc<AttachmentHandle> &); // should be called from GL thread
	virtual void setInputSubmitted(const Rc<AttachmentHandle> &); // should be called from GL thread
	virtual void setRenderPassPrepared(const Rc<RenderPassHandle> &); // should be called from GL thread
	virtual void setRenderPassSubmitted(const Rc<RenderPassHandle> &); // should be called from GL thread

	virtual void submitRenderPass(const Rc<RenderPassHandle> &);

	virtual bool isReadyForSubmit() const { return _readyForSubmit; }
	virtual void setReadyForSubmit(bool);

	virtual void invalidate();

protected:
	virtual void releaseResources();
	virtual void releaseRenderPassResources(const Rc<RenderPass> &, const Rc<SwapchainAttachment> &);

	Loop *_loop = nullptr; // loop can not die until frames are performed
	Device *_device = nullptr;// device can not die until frames are performed
	Rc<RenderQueue> _queue; // hard reference to render queue, it should not be released until at least one frame uses it

	uint64_t _order = 0;
	uint32_t _gen = 0;
	uint32_t _inputs = 0;
	uint32_t _inputSubmitted = 0;
	bool _readyForSubmit = false;
	bool _submitted = false;
	bool _valid = true;
	Vector<Rc<AttachmentHandle>> _availableAttachments;
	Vector<Rc<AttachmentHandle>> _requiredAttachments;
	Vector<Rc<AttachmentHandle>> _inputAttachments;
	Vector<Rc<AttachmentHandle>> _readyAttachments;

	Vector<Rc<RenderPassHandle>> _requiredRenderPasses;
	Vector<Rc<RenderPassHandle>> _preparedRenderPasses;
	Vector<Rc<RenderPassHandle>> _submittedRenderPasses;

	Map<Rc<RenderPassHandle>, Rc<SwapchainAttachment>> _swapchainAttachments;
};

/*struct FrameData : public Ref {
	virtual ~FrameData();

	Rc<gl::Device> device;
	Rc<gl::Loop> loop;

	FrameStatus status = FrameStatus::ImageAcquired;

	// frame info
	Rc<OptionsContainer> options;
	Rc<FrameSync> sync;
	// Vector<VkPipelineStageFlags> waitStages;

	// target image info
	Rc<Framebuffer> framebuffer;
	uint32_t imageIdx;

	// data
	Vector<VkCommandBuffer> buffers;
	Vector<CommandBufferTask> tasks;
	uint32_t gen = 0;
	uint64_t order = 0;
};*/


}

#endif /* XENOLITH_GL_COMMON_XLGLFRAME_H_ */
