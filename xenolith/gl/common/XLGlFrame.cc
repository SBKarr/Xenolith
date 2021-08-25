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

FrameHandle::~FrameHandle() { }

bool FrameHandle::init(Loop &loop, RenderQueue &queue, uint64_t order, uint32_t gen, bool readyForSubmit) {
	_loop = &loop;
	_device = loop.getDevice();
	_queue = &queue;
	_order = order;
	_gen = gen;
	_readyForSubmit = readyForSubmit;

	for (auto &it : _queue->getAttachments()) {
		auto h = it->makeFrameHandle(*this);
		if (h->setup(*this)) {
			if (h->isInput()) {
				_inputAttachments.emplace_back(h);
			} else {
				_readyAttachments.emplace_back(h);
			}
			h->setReady(true);
		}
		_requiredAttachments.emplace_back(h);
	}

	_requiredRenderPasses.reserve(_queue->getPasses().size());
	for (auto &it : _queue->getPasses()) {
		_requiredRenderPasses.emplace_back(it->renderPass->makeFrameHandle(it, *this));
	}

	for (auto &it : _requiredRenderPasses) {
		it->buildRequirements(*this, _requiredRenderPasses, _requiredAttachments);
	}

	update();

	return true;
}

void FrameHandle::update() {
	do {
		auto it = _requiredRenderPasses.begin();
		while (it != _requiredRenderPasses.end()) {
			if ((*it)->isReady()) {
				auto pass = (*it);
				pass->run(*this);
				it = _requiredRenderPasses.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	do {
		auto it = _preparedRenderPasses.begin();
		while (it != _preparedRenderPasses.end()) {
			if ((*it)->isAsync() || _readyForSubmit) {
				auto pass = (*it);
				pass->submit(*this);
				it = _preparedRenderPasses.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);
}

void FrameHandle::schedule(Function<bool(FrameHandle &, Loop::Context &)> &&cb) {
	retain();
	_loop->schedule([this, cb = move(cb)] (Loop::Context &ctx) {
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

void FrameHandle::performOnGlThread(Function<void(FrameHandle &)> &&cb, Ref *ref) {
	retain();
	_loop->getQueue()->onMainThread(Rc<thread::Task>::create([this, cb = move(cb)] (const thread::Task &, bool success) {
		if (success) { cb(*this); }
		release();
	}, ref));
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
		return;
	}
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
		return;
	}

	if (pass->isAsync() || _readyForSubmit) {
		// should not wait for all previous frame submits
		pass->submit(*this);
	} else {
		_preparedRenderPasses.emplace_back(pass);
	}
}

void FrameHandle::setRenderPassSubmitted(const Rc<RenderPassHandle> &handle) {
	if (!isValid()) {
		return;
	}

	_submittedRenderPasses.emplace_back(handle);

	if (_submittedRenderPasses.size() == _queue->getPasses().size()) {
		// set next frame ready for submit
		_loop->getDevice()->setFrameSubmitted(this);
	}
}

void FrameHandle::setReadyForSubmit(bool value) {
	_readyForSubmit = value;
	if (_readyForSubmit) {
		_loop->pushContextEvent(Loop::Event::FrameUpdate, this);
	}
}

void FrameHandle::invalidate() {
	_valid = false;
	_device->invalidateFrame(*this);
}

}
