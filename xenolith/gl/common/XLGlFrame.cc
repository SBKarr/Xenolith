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

uint32_t FrameHandle::GetActiveFramesCount() {
	return s_frameCount.load();
}

FrameHandle::~FrameHandle() {
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] Destroy");
	-- s_frameCount;
	releaseResources();

	if (_queue) {
		_queue->endFrame(*this);
		_queue = nullptr;
	}
}

bool FrameHandle::init(Loop &loop, Swapchain &swapchain, RenderQueue &queue, uint64_t order, uint32_t gen, bool readyForSubmit) {
	++ s_frameCount;
	_loop = &loop;
	_order = order;
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] Init (", loop.getDevice()->getFramesActive(), ")");
	_device = loop.getDevice();
	_swapchain = &swapchain;
	_queue = &queue;
	_gen = gen;
	_readyForSubmit = readyForSubmit;
	return setup();
}

bool FrameHandle::init(Loop &loop, RenderQueue &queue, uint64_t order, uint32_t gen) {
	++ s_frameCount;
	_loop = &loop;
	_order = order;
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] Init (", loop.getDevice()->getFramesActive(), ")");
	_device = loop.getDevice();
	_swapchain = nullptr;
	_queue = &queue;
	_gen = gen;
	_readyForSubmit = true;
	return setup();
}

void FrameHandle::update(bool init) {
	if (!init && !isValid()) {
		releaseResources();
	}

	if (!_valid) {
		return;
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
						_queue->acquireInput(*this, (*it));
					} else {
						_readyAttachments.emplace_back((*it));
						(*it)->setReady(true);
					}
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
	auto linkId = retain();
	_loop->schedule([this, cb = move(cb), linkId] (Loop::Context &ctx) {
		if (!isValid()) {
			release(linkId);
			return true;
		}
		if (cb(*this, ctx)) {
			release(linkId);
			return true; // end
		}
		return false;
	});
}

void FrameHandle::performInQueue(Function<void(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &) -> bool {
		cb(*this);
		return true;
	}, [this, linkId, tag] (const thread::Task &, bool) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performInQueue(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete,
		Ref *ref, StringView tag) {
	auto linkId = retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, perform = move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [this, complete = move(complete), linkId, tag] (const thread::Task &, bool success) {
		complete(*this, success);
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
		release(linkId);
	}, ref));
}

void FrameHandle::performOnGlThread(Function<void(FrameHandle &)> &&cb, Ref *ref, bool immediate, StringView tag) {
	if (immediate && _loop->isOnThread()) {
		cb(*this);
	} else {
		auto linkId = retain();
		_loop->getQueue()->onMainThread(Rc<thread::Task>::create([this, cb = move(cb), linkId, tag] (const thread::Task &, bool success) {
			if (success) { cb(*this); }
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
			release(linkId);
		}, ref));
	}
}

void FrameHandle::performRequiredTask(Function<void(FrameHandle &)> &&cb, Ref *ref, StringView tag) {
	++ _tasksRequired;
	auto linkId = retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &) -> bool {
		cb(*this);
		return true;
	}, [this, linkId, tag] (const thread::Task &, bool) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
		onRequiredTaskCompleted(tag);
		release(linkId);
	}, ref));
}

void FrameHandle::performRequiredTask(Function<bool(FrameHandle &)> &&perform, Function<void(FrameHandle &, bool)> &&complete,
		Ref *ref, StringView tag) {
	++ _tasksRequired;
	auto linkId = retain();
	_loop->getQueue()->perform(Rc<thread::Task>::create([this, perform = move(perform)] (const thread::Task &) -> bool {
		return perform(*this);
	}, [this, complete = move(complete), linkId, tag] (const thread::Task &, bool success) {
		complete(*this, success);
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] thread performed: '", tag, "'");
		onRequiredTaskCompleted(tag);
		release(linkId);
	}, ref));
}

bool FrameHandle::isInputRequired() const {
	return !_inputAttachments.empty();
}

bool FrameHandle::isPresentable() const {
	return _queue->isPresentable();
}

bool FrameHandle::isValid() const {
	return _valid && (!_swapchain || _swapchain->isFrameValid(*this));
}

bool FrameHandle::submitInput(const Rc<AttachmentHandle> &attachemnt, Rc<AttachmentInputData> &&data) {
	if (attachemnt->isInput()) {
		if (attachemnt->submitInput(*this, move(data))) {
			++ _inputSubmitted;
			return true;
		}
	}
	return false;
}

bool FrameHandle::submitInput(const Attachment *a, Rc<AttachmentInputData> &&data) {
	for (auto &it : _inputAttachments) {
		if (it->getAttachment() == a) {
			return submitInput(it, move(data));
		}
	}
	return false;
}

void FrameHandle::setAttachmentReady(const Rc<AttachmentHandle> &handle) {
	if (!isValid()) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [invalid] attachment ready '", handle->getAttachment()->getName(), "'");
		return;
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] attachment ready '", handle->getAttachment()->getName(), "'");
	if (handle->isInput()) {
		_inputAttachments.emplace_back(handle);
		_queue->acquireInput(*this, handle);
	} else {
		_readyAttachments.emplace_back(handle);
		handle->setReady(true);
	}
	_loop->pushContextEvent(Loop::EventName::FrameUpdate, this);
}

void FrameHandle::setInputSubmitted(const Rc<AttachmentHandle> &handle) {
	if (handle->isInput()) {
		_readyAttachments.emplace_back(handle);
		handle->setReady(true);
		_loop->pushContextEvent(Loop::EventName::FrameUpdate, this);
	}
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
		auto linkId = retain();
		releaseResources();
		if (_swapchain) {
			_swapchain->setFrameSubmitted(this);
		}
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] frame submitted");
		release(linkId);
	}
}

void FrameHandle::submitRenderPass(const Rc<RenderPassHandle> &pass) {
	if (pass->getData()->isPresentable) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] pre-submit '", pass->getRenderPass()->getName(), "'");
		_submitted = true;
		if (_swapchain) {
			_loop->pushContextEvent(gl::Loop::EventName::FrameSubmitted, _swapchain);
		}
	}

	Rc<SwapchainAttachment> a;
	auto attachmentIt = _swapchainAttachments.find(pass);
	if (attachmentIt != _swapchainAttachments.end()) {
		a = attachmentIt->second;
		_swapchainAttachments.erase(attachmentIt);
	}

	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] submit render pass '", pass->getRenderPass()->getName(), "'");
	auto id = retain();
	++ _renderPassInProgress;
	pass->submit(*this, [this, a, id] (const Rc<RenderPass> &pass) {
		releaseRenderPassResources(pass, a);
		-- _renderPassInProgress;
		setRenderPassComplete(pass);
		release(id);
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
		_loop->pushContextEvent(Loop::EventName::FrameUpdate, this);
	}
}

void FrameHandle::invalidate() {
	if (_loop->isOnThread()) {
		if (_valid) {
			auto linkId = retain();
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] frame invalidated");
			_valid = false;
			_device->invalidateFrame(*this);
			releaseResources();
			_loop->autorelease(this);
			if (_queue) {
				_queue->endFrame(*this);
				_queue = nullptr;
			}
			release(linkId);
		}
	} else {
		_loop->performOnThread([this] {
			invalidate();
		}, this);
	}
}

void FrameHandle::setCompleteCallback(Function<void(FrameHandle &)> &&cb) {
	_complete = move(cb);
}

bool FrameHandle::setup() {
	_requiredRenderPasses.reserve(_queue->getPasses().size());
	for (auto &it : _queue->getPasses()) {
		_requiredRenderPasses.emplace_back(it->renderPass->makeFrameHandle(it, *this));
	}

	_queue->beginFrame(*this);

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
		if (h->isOutput()) {
			_outputAttachments.emplace_back(h);
		}

		_availableAttachments.emplace_back(h);

		/*if (h->isAvailable(*this)) {
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] setup attachment '", h->getAttachment()->getName(), "'");
			if (h->setup(*this)) {
				XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] attachment ready after setup '", h->getAttachment()->getName(), "'");
				if (h->isInput()) {
					_inputAttachments.emplace_back(h);
					_queue->acquireInput(*this, h);
				} else {
					_readyAttachments.emplace_back(h);
					h->setReady(true);
				}
			}
		} else {
			XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] wait available '", h->getAttachment()->getName(), "'");

		}*/
	}

	_renderPassRequired = _requiredRenderPasses.size();

	for (auto &it : _requiredRenderPasses) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] build render pass '", it->getRenderPass()->getName(), "'");
		it->buildRequirements(*this, _requiredRenderPasses, _requiredAttachments);
	}

	if (!_valid) {
		releaseResources();
	}

	return true;
}

void FrameHandle::releaseResources() {
	if (!_swapchainAttachments.empty()) {
	 	for (auto &it : _swapchainAttachments) {
			if (it.second->releaseForFrame(*this)) {
				XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [forced] release swapchain '", it.second->getName(), "'");
			}
		}
		_swapchainAttachments.clear();
	}

	if (!_preparedRenderPasses.empty()) {
		for (auto &it : _preparedRenderPasses) {
			if (it->getRenderPass()->releaseForFrame(*this)) {
				XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] [forced] release render pass '", it->getName(), "'");
			}
		}
		_preparedRenderPasses.clear();
	}
}

void FrameHandle::releaseRenderPassResources(const Rc<RenderPass> &pass, const Rc<SwapchainAttachment> &a) {
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] release render pass '", pass->getName(), "'");
	pass->releaseForFrame(*this);
	if (a) {
		XL_FRAME_LOG("[", _loop->getClock(), "] [", _order, "] [", s_frameCount.load(), "] release swapchain '", a->getName(), "'");
		a->releaseForFrame(*this);
	}
}

void FrameHandle::setRenderPassComplete(const Rc<RenderPass> &pass) {
	++ _renderPassCompleted;
	if (_tasksCompleted == _tasksRequired.load() && _renderPassCompleted == _renderPassRequired) {
		onComplete();
	}
}

void FrameHandle::onRequiredTaskCompleted(StringView tag) {
	++ _tasksCompleted;
	if (_tasksCompleted == _tasksRequired.load() && _renderPassCompleted == _renderPassRequired) {
		onComplete();
	}
}

void FrameHandle::onComplete() {
	if (!_completed) {
		_completed = true;
		if (_complete) {
			_complete(*this);
		}
	}
}

}
