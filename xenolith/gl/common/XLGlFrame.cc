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

#include "XLGlFrame.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::gl {

static std::atomic<uint32_t> s_frameCount = 0;

FrameHandle::~FrameHandle() {
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] Destroy");
	-- s_frameCount;
	releaseResources();
}

bool FrameHandle::init(Loop &loop, RenderQueue &queue, uint64_t order, uint32_t gen, bool readyForSubmit) {
	++ s_frameCount;
	_loop = &loop;
	_order = order;
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] Init (", loop.getDevice()->getFramesActive(), ")");
	_device = loop.getDevice();
	_queue = &queue;
	_gen = gen;
	_readyForSubmit = readyForSubmit;

	_requiredRenderPasses.reserve(_queue->getPasses().size());
	for (auto &it : _queue->getPasses()) {
		_requiredRenderPasses.emplace_back(it->renderPass->makeFrameHandle(it, *this));
	}

	for (auto &it : _queue->getAttachments()) {
		if (it->getType() == AttachmentType::SwapchainImage) {
			auto a = (SwapchainAttachment *)it.get();
			auto lastPass = a->getLastRenderPass();
			if (lastPass->isPresentable) {
				for (auto &p : _requiredRenderPasses) {
					if (p->getData() == lastPass) {
						a->acquireForFrame(*this);
						XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] acquireForFrame '", a->getName(), "'");
						_swapchainAttachments.emplace(p, a);
					}
				}
			}
		}
		auto h = it->makeFrameHandle(*this);
		_requiredAttachments.emplace_back(h);
		if (h->isAvailable(*this)) {
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] setup attachment '", h->getAttachment()->getName(), "'");
			if (h->setup(*this)) {
				XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] attachment ready after setup '", h->getAttachment()->getName(), "'");
				if (h->isInput()) {
					_inputAttachments.emplace_back(h);
				} else {
					_readyAttachments.emplace_back(h);
				}
				h->setReady(true);
			}
		} else {
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] wait available '", h->getAttachment()->getName(), "'");
			_availableAttachments.emplace_back(h);
		}
	}

	for (auto &it : _requiredRenderPasses) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] build render pass '", it->getRenderPass()->getName(), "'");
		it->buildRequirements(*this, _requiredRenderPasses, _requiredAttachments);
	}

	if (_valid) {
		update(true);
	}

	if (!_valid) {
		releaseResources();
	}

	return true;
}

void FrameHandle::update(bool init) {
	if (!init && !isValid()) {
		releaseResources();
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] update");
	do {
		auto it = _availableAttachments.begin();
		while (it != _availableAttachments.end()) {
			if ((*it)->isAvailable(*this)) {
				XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] setup attachment '", (*it)->getAttachment()->getName(), "'");
				if ((*it)->setup(*this)) {
					XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] attachment ready after setup '", (*it)->getAttachment()->getName(), "'");
					if ((*it)->isInput()) {
						_inputAttachments.emplace_back((*it));
					} else {
						_readyAttachments.emplace_back((*it));
					}
					(*it)->setReady(true);
				}
				it = _availableAttachments.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	do {
		auto it = _requiredRenderPasses.begin();
		while (it != _requiredRenderPasses.end()) {
			if ((*it)->isReady()) {
				auto pass = (*it);
				XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] prepare render pass '", pass->getRenderPass()->getName(), "'");
				pass->prepare(*this);
				it = _requiredRenderPasses.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	do {
		auto it = _preparedRenderPasses.begin();
		while (it != _preparedRenderPasses.end()) {
			if ((*it)->isAsync() || (_readyForSubmit && (*it)->isAvailable(*this))) {
				auto pass = (*it);
				it = _preparedRenderPasses.erase(it);
				submitRenderPass(pass);
			} else {
				++ it;
			}
		}
	} while (0);
}

void FrameHandle::schedule(Function<bool(FrameHandle &, Loop::Context &)> &&cb) {
	retain();
	_loop->schedule([this, cb = move(cb)] (Loop::Context &ctx) {
		if (!isValid()) {
			release();
			return true;
		}
		if (cb(*this, ctx)) {
			release();
			return true; // end
		}
		return false;
	});
}

void FrameHandle::performInQueue(Function<void(FrameHandle &)> &&cb, Ref *ref) {
	retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &) -> bool {
		cb(*this);
		return true;
	}, [this] (const thread::Task &, bool) {
		release();
	}, ref));
}

void FrameHandle::performInQueue(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete, Ref *ref) {
	retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, perform = move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [this, complete = move(complete)] (const thread::Task &, bool success) {
		complete(*this, success);
		release();
	}, ref));
}

void FrameHandle::performOnGlThread(Function<void(FrameHandle &)> &&cb, Ref *ref, bool immediate) {
	if (immediate && _loop->isOnThread()) {
		cb(*this);
	} else {
		retain();
		_loop->getQueue()->onMainThread(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &, bool success) {
			if (success) { cb(*this); }
			release();
		}, ref));
	}
}

bool FrameHandle::isInputRequired() const {
	return !_inputAttachments.empty();
}

bool FrameHandle::isPresentable() const {
	return _queue->isPresentable();
}

bool FrameHandle::isValid() const {
	return _valid && _device->isFrameValid(*this);
}

void FrameHandle::setAttachmentReady(const Rc<AttachmentHandle> &handle) {
	if (!isValid()) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [invalid] attachment ready '", handle->getAttachment()->getName(), "'");
		return;
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] attachment ready '", handle->getAttachment()->getName(), "'");
	if (handle->isInput()) {
		_inputAttachments.emplace_back(handle);
	} else {
		_readyAttachments.emplace_back(handle);
	}
	handle->setReady(true);
	_loop->pushContextEvent(Loop::Event::FrameUpdate, this);
}

void FrameHandle::setRenderPassPrepared(const Rc<RenderPassHandle> &pass) {
	if (!isValid()) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [invalid] render pass prepared '", pass->getRenderPass()->getName(), "'");
		return;
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] render pass prepared '", pass->getRenderPass()->getName(), "'");
	pass->getRenderPass()->acquireForFrame(*this);
	if (pass->isAsync() || (_readyForSubmit && pass->isAvailable(*this))) {
		submitRenderPass(pass);
	} else {
		_preparedRenderPasses.emplace_back(pass);
	}
}

void FrameHandle::setRenderPassSubmitted(const Rc<RenderPassHandle> &handle) {
	if (!isValid()) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [invalid] render pass submited '", handle->getRenderPass()->getName(), "'");
		return;
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] render pass submited '", handle->getRenderPass()->getName(), "'");
	_submittedRenderPasses.emplace_back(handle);

	if (_submittedRenderPasses.size() == _queue->getPasses().size()) {
		// set next frame ready for submit
		retain();
		releaseResources();
		_loop->getDevice()->setFrameSubmitted(this);
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] frame submitted");
		release();
	}
}

void FrameHandle::submitRenderPass(const Rc<RenderPassHandle> &pass) {
	if (pass->getData()->isPresentable) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] pre-submit '", pass->getRenderPass()->getName(), "'");
		_submitted = true;
		_loop->pushContextEvent(gl::Loop::Event::FrameSubmitted);
	}

	Rc<SwapchainAttachment> a;
	auto attachmentIt = _swapchainAttachments.find(pass);
	if (attachmentIt != _swapchainAttachments.end()) {
		a = attachmentIt->second;
		_swapchainAttachments.erase(attachmentIt);
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] submit render pass '", pass->getRenderPass()->getName(), "'");
	pass->submit(*this, [this, a] (const Rc<RenderPass> &pass) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] release render pass '", pass->getName(), "'");
		pass->releaseForFrame(*this);
		if (a) {
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] release swapchain '", a->getName(), "'");
			a->releaseForFrame(*this);
		}
	});
}

void FrameHandle::setReadyForSubmit(bool value) {
	if (!isValid()) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [invalid] frame ready to submit");
		return;
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] frame ready to submit");
	_readyForSubmit = value;
	if (_readyForSubmit) {
		_loop->pushContextEvent(Loop::Event::FrameUpdate, this);
	}
}

void FrameHandle::invalidate() {
	if (_valid) {
		retain();
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] frame invalidated");
		_valid = false;
		_device->invalidateFrame(*this);
		releaseResources();
		_loop->autorelease(this);
		release();
	}
}

void FrameHandle::releaseResources() {
	for (auto &it : _swapchainAttachments) {
		if (it.second->releaseForFrame(*this)) {
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [forced] release swapchain '", it.second->getName(), "'");
		}
	}
	_swapchainAttachments.clear();

	for (auto &it : _preparedRenderPasses) {
		if (it->getRenderPass()->releaseForFrame(*this)) {
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [forced] release render pass '", it->getName(), "'");
		}
	}
	_preparedRenderPasses.clear();
}

}
