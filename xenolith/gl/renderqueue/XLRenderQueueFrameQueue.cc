/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLGlLoop.h"
#include "XLRenderQueueFrameQueue.h"
#include "XLRenderQueueFrameHandle.h"
#include "XLRenderQueueImageStorage.h"
#include "XLRenderQueueQueue.h"

namespace stappler::xenolith::renderqueue {

FrameQueue::~FrameQueue() {
	_frame = nullptr;
	XL_FRAME_QUEUE_LOG("Ended");
}

bool FrameQueue::init(const Rc<PoolRef> &p, const Rc<Queue> &q, FrameHandle &f, Extent2 ext) {
	_pool = p;
	_queue = q;
	_frame = &f;
	_loop = _frame->getLoop();
	_extent = ext;
	_order = f.getOrder();
	XL_FRAME_QUEUE_LOG("Started");
	return true;
}

bool FrameQueue::setup() {
	bool valid = true;

	_renderPasses.reserve(_queue->getPasses().size());
	_renderPassesInitial.reserve(_queue->getPasses().size());

	for (auto &it : _queue->getPasses()) {
		Extent2 extent = it->renderPass->getSizeForFrame(*this);
		auto pass = it->renderPass->makeFrameHandle(*this);
		if (pass->isAvailable(*this)) {
			auto v = _renderPasses.emplace(it, FramePassData{
				FrameRenderPassState::Initial,
				pass,
				extent
			}).first;
			pass->setQueueData(v->second);
			_renderPassesInitial.emplace(&v->second);
		}
	}

	_attachments.reserve(_queue->getAttachments().size());
	_attachmentsInitial.reserve(_queue->getAttachments().size());

	for (auto &it : _queue->getAttachments()) {
		Extent3 extent = _extent;
		if (it->type == AttachmentType::Image) {
			auto img = (ImageAttachment *)it->attachment.get();
			extent = img->getSizeForFrame(*this);
		}
		auto h = it->attachment->makeFrameHandle(*this);
		if (h->isAvailable(*this)) {
			auto v = _attachments.emplace(it, FrameAttachmentData({
				FrameAttachmentState::Initial,
				h,
				extent
			})).first;
			h->setQueueData(v->second);
			_attachmentsInitial.emplace(&v->second);
		}
	}

	for (auto &it : _attachments) {
		auto passes = it.second.handle->getAttachment()->getRenderPasses();
		it.second.passes.reserve(passes.size());
		for (auto &pass : passes) {
			auto passIt = _renderPasses.find(pass);
			if (passIt != _renderPasses.end()) {
				it.second.passes.emplace_back(&passIt->second);
			} else {
				XL_FRAME_QUEUE_LOG("RenderPass '", pass->key, "' is not available on frame");
				valid = false;
			}
		}

		auto &last = it.first->passes.back();
		it.second.final = last->dependency.requiredRenderPassState;
	}

	for (auto &passIt : _renderPasses) {
		for (auto &a : passIt.first->attachments) {
			auto aIt = _attachments.find(a->attachment);
			if (aIt != _attachments.end()) {
				passIt.second.attachments.emplace_back(a, &aIt->second);
			} else {
				XL_FRAME_QUEUE_LOG("Attachment '", a->key, "' is not available on frame");
				valid = false;
			}
		}

		for (auto &a : passIt.first->attachments) {
			auto aIt = _attachments.find(a->attachment);
			if (aIt != _attachments.end()) {
				if (a->index == maxOf<uint32_t>()) {
					passIt.second.attachments.emplace_back(a, &aIt->second);
				}
			} else {
				XL_FRAME_QUEUE_LOG("Attachment '", a->key, "' is not available on frame");
				valid = false;
			}
		}

		for (auto &a : passIt.second.attachments) {
			auto &desc = a.first->attachment->passes;
			auto it = desc.begin();
			while (it != desc.end() && (*it)->pass != passIt.second.handle->getData()) {
				auto iit = _renderPasses.find((*it)->pass);
				if (iit != _renderPasses.end()) {
					addRequiredPass(passIt.second, iit->second, *a.second, **it);
					++ it;
				} else {
					XL_FRAME_QUEUE_LOG("RenderPass '", (*it)->pass->key, "' is not available on frame");
					valid = false;
					break;
				}
			}

			passIt.second.attachmentMap.emplace(a.first->attachment, a.second);
		}
	}

	for (auto &passIt : _renderPasses) {
		for (auto &it : passIt.second.required) {
			auto wIt = it.data->waiters.find(it.requiredState);
			if (wIt == it.data->waiters.end()) {
				wIt = it.data->waiters.emplace(it.requiredState, Vector<FramePassData *>()).first;
			}
			wIt->second.emplace_back(&passIt.second);
		}
	}

	XL_FRAME_QUEUE_LOG("Setup: ", valid);
	return valid;
}

void FrameQueue::update() {
	for (auto &it : _attachmentsInitial) {
		if (it->handle->setup(*this, [this, guard = Rc<FrameQueue>(this), attachment = it] (bool success) {
			_loop->performOnGlThread([this, attachment, success] {
				attachment->waitForResult = false;
				if (success && !_finalized) {
					onAttachmentSetupComplete(*attachment);
					_loop->performOnGlThread([this] {
						if (_frame) {
							_frame->update();
						}
					}, this);
				} else {
					invalidate(*attachment);
				}
			}, guard, true);
		})) {
			onAttachmentSetupComplete(*it);
		} else {
			it->waitForResult = true;
			XL_FRAME_QUEUE_LOG("[Attachment:", it->handle->getName(), "] State: Setup");
			it->state = FrameAttachmentState::Setup;
		}
	}
	_attachmentsInitial.clear();

	do {
		auto it = _renderPassesInitial.begin();
		while (it != _renderPassesInitial.end()) {
			if ((*it)->state == FrameRenderPassState::Initial) {
				if (isRenderPassReady(**it)) {
					auto v = *it;
					it = _renderPassesInitial.erase(it);
					updateRenderPassState(*v, FrameRenderPassState::Ready);
				} else {
					++ it;
				}
			} else {
				it = _renderPassesInitial.erase(it);
			}
		}
	} while (0);

	do {
		auto awaitPasses = move(_awaitPasses);
		_awaitPasses.clear();
		_awaitPasses.reserve(awaitPasses.size());

		auto it = awaitPasses.begin();
		while (it != awaitPasses.end()) {
			if (isRenderPassReadyForState(*it->first, FrameRenderPassState(toInt(it->second) + 1))) {
				updateRenderPassState(*it->first, it->second);
			} else {
				_awaitPasses.emplace_back(*it);
			}
			++ it;
		}
	} while (0);

	do {
		auto it = _renderPassesPrepared.begin();
		while (it != _renderPassesPrepared.end()) {
			if ((*it)->state == FrameRenderPassState::Prepared) {
				auto v = *it;
				onRenderPassPrepared(*v);
				if (v->state != FrameRenderPassState::Prepared) {
					it = _renderPassesPrepared.erase(it);
				} else {
					++ it;
				}
			} else {
				it = _renderPassesPrepared.erase(it);
			}
		}
	} while(0);
}

void FrameQueue::invalidate() {
	if (!_finalized) {
		XL_FRAME_QUEUE_LOG("invalidate");
		_success = false;
		auto f = _frame;
		onFinalized();
		if (f) {
			f->onQueueInvalidated(*this);
			tryReleaseFrame();
		}
	}
}

gl::Loop *FrameQueue::getLoop() const {
	return _loop;
}

const FrameAttachmentData *FrameQueue::getAttachment(const AttachmentData *a) const {
	auto it = _attachments.find(a);
	if (it != _attachments.end()) {
		return &it->second;
	}
	return nullptr;
}

const FramePassData *FrameQueue::getRenderPass(const PassData *p) const {
	auto it = _renderPasses.find(p);
	if (it != _renderPasses.end()) {
		return &it->second;
	}
	return nullptr;
}

void FrameQueue::addRequiredPass(FramePassData &pass, const FramePassData &required,
		const FrameAttachmentData &attachment, const AttachmentPassData &desc) {
	auto requiredState = desc.dependency.requiredRenderPassState;
	auto lockedState = desc.dependency.lockedRenderPassState;
	if (requiredState == FrameRenderPassState::Initial) {
		return;
	}

	auto lb = std::lower_bound(pass.required.begin(), pass.required.end(), &required,
			[&] (const FramePassDataRequired &l, const FramePassData *r) {
		return l.data < r;
	});
	if (lb == pass.required.end()) {
		pass.required.emplace_back((FramePassData *)&required, requiredState, lockedState);
	} else if (lb->data != &required) {
		pass.required.emplace(lb, (FramePassData *)&required, requiredState, lockedState);
	} else {
		lb->requiredState = FrameRenderPassState(std::max(toInt(lb->requiredState), toInt(requiredState)));
		lb->lockedState = FrameRenderPassState(std::min(toInt(lb->lockedState), toInt(lockedState)));
	}
}

bool FrameQueue::isResourcePending(const FrameAttachmentData &image) {
	if (image.image) {
		if (!image.image->isReady()) {
			return true;
		}
	}
	return false;
}

void FrameQueue::waitForResource(const FrameAttachmentData &image, Function<void(bool)> &&cb) {
	if (image.image) {
		image.image->waitReady(move(cb));
	}
}

bool FrameQueue::isResourcePending(const FramePassData &) {
	return false;
}

void FrameQueue::waitForResource(const FramePassData &, Function<void()> &&) {
	// TODO
}

void FrameQueue::onAttachmentSetupComplete(FrameAttachmentData &attachment) {
	if (attachment.handle->isOutput()) {
		// do nothing for now
	}
	if (attachment.handle->isInput()) {
		XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: InputRequired");
		attachment.state = FrameAttachmentState::InputRequired;
		if (auto data = _frame->getInputData(attachment.handle->getAttachment()->getData())) {
			attachment.waitForResult = true;
			attachment.handle->submitInput(*this, move(data),
					[this, guard = Rc<FrameQueue>(this), attachment = &attachment] (bool success) {
				_loop->performOnGlThread([this, attachment, success] {
					attachment->waitForResult = false;
					if (success && !_finalized) {
						onAttachmentInput(*attachment);
						_loop->performOnGlThread([this] {
							if (_frame) {
								_frame->update();
							}
						}, this);
					} else {
						invalidate(*attachment);
					}
				}, guard, true);
			});
		} else {
			attachment.waitForResult = true;
			attachment.handle->getAttachment()->acquireInput(*this, attachment.handle,
					[this, guard = Rc<FrameQueue>(this), attachment = &attachment] (bool success) {
				_loop->performOnGlThread([this, attachment, success] {
					attachment->waitForResult = false;
					if (success && !_finalized) {
						onAttachmentInput(*attachment);
						_loop->performOnGlThread([this] {
							if (_frame) {
								_frame->update();
							}
						}, this);
					} else {
						invalidate(*attachment);
					}
				}, guard, true);
			});
		}
	} else {
		XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: Ready");
		attachment.state = FrameAttachmentState::Ready;
	}
}

void FrameQueue::onAttachmentInput(FrameAttachmentData &attachment) {
	XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: Ready");
	attachment.state = FrameAttachmentState::Ready;
}

void FrameQueue::onAttachmentAcquire(FrameAttachmentData &attachment) {
	if (_finalized) {
		if (attachment.state != FrameAttachmentState::Finalized) {
			finalizeAttachment(attachment);
		}
		return;
	}

	XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: ResourcesPending");
	attachment.state = FrameAttachmentState::ResourcesPending;
	if (attachment.handle->getAttachment()->getData()->type == AttachmentType::Image) {
		auto img = (ImageAttachment *)attachment.handle->getAttachment().get();

		attachment.image = _frame->getRenderTarget(attachment.handle->getAttachment());

		if (!attachment.image && attachment.handle->isAvailable(*this)) {
			attachment.image = _loop->acquireImage(img, attachment.handle.get(), attachment.extent);
			if (!attachment.image) {
				invalidate(attachment);
				return;
			}

			attachment.image->setFrameIndex(_frame->getOrder());

			_autorelease.emplace_front(attachment.image);
			if (attachment.image->getSignalSem()) {
				_autorelease.emplace_front(attachment.image->getSignalSem());
			}
			if (attachment.image->getWaitSem()) {
				_autorelease.emplace_front(attachment.image->getWaitSem());
			}
		}

		if (isResourcePending(attachment)) {
			waitForResource(attachment, [this, attachment = &attachment] (bool success) {
				if (!success) {
					invalidate();
					return;
				}
				XL_FRAME_QUEUE_LOG("[Attachment:", attachment->handle->getName(), "] State: ResourcesAcquired");
				attachment->state = FrameAttachmentState::ResourcesAcquired;
			});
		} else {
			XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: ResourcesAcquired");
			attachment.state = FrameAttachmentState::ResourcesAcquired;
		}
	} else {
		XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: ResourcesAcquired");
		attachment.state = FrameAttachmentState::ResourcesAcquired;
	}
}

void FrameQueue::onAttachmentRelease(FrameAttachmentData &attachment) {
	if (attachment.image) {
		if (attachment.handle->getAttachment()->getData()->type == AttachmentType::Image) {
			if (attachment.image) {
				_loop->releaseImage(move(attachment.image));
				attachment.image = nullptr;
			}
		}

		if (_finalized && attachment.state != FrameAttachmentState::Finalized) {
			finalizeAttachment(attachment);
		} else {
			XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: ResourcesReleased");
			attachment.state = FrameAttachmentState::ResourcesReleased;
		}
	} else {
		if (_finalized && attachment.state != FrameAttachmentState::Finalized) {
			finalizeAttachment(attachment);
		} else {
			XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: ResourcesReleased");
			attachment.state = FrameAttachmentState::ResourcesReleased;
		}
	}
}

bool FrameQueue::isRenderPassReady(const FramePassData &data) const {
	return isRenderPassReadyForState(data, FrameRenderPassState::Initial);
}

bool FrameQueue::isRenderPassReadyForState(const FramePassData &data, FrameRenderPassState state) const {
	for (auto &it : data.required) {
		if (toInt(it.data->state) < toInt(it.requiredState)) {
			if (state >= it.lockedState) {
				return false;
			}
		}
	}

	for (auto &it : data.attachments) {
		if (toInt(it.second->state) < toInt(FrameAttachmentState::Ready)) {
			return false;
		}
	}
	return true;
}

void FrameQueue::updateRenderPassState(FramePassData &data, FrameRenderPassState state) {
	if (_finalized && state != FrameRenderPassState::Finalized) {
		return;
	}

	if (state == FrameRenderPassState::Ready && data.handle->isAsync()) {
		state = FrameRenderPassState::Owned;
	}

	if (toInt(data.state) >= toInt(state)) {
		return;
	}

	if (!isRenderPassReadyForState(data, FrameRenderPassState(toInt(state) + 1))) {
		_awaitPasses.emplace_back(&data, state);
		return;
	}

	data.state = state;

	switch (state) {
	case FrameRenderPassState::Initial:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Initial");
		break;
	case FrameRenderPassState::Ready:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Ready");
		onRenderPassReady(data);
		break;
	case FrameRenderPassState::Owned:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Owned");
		onRenderPassOwned(data);
		break;
	case FrameRenderPassState::ResourcesAcquired:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: ResourcesAcquired");
		onRenderPassResourcesAcquired(data);
		break;
	case FrameRenderPassState::Prepared:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Prepared");
		onRenderPassPrepared(data);
		break;
	case FrameRenderPassState::Submission:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Submission");
		onRenderPassSubmission(data);
		break;
	case FrameRenderPassState::Submitted:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Submitted");
		onRenderPassSubmitted(data);
		break;
	case FrameRenderPassState::Complete:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Complete");
		onRenderPassComplete(data);
		break;
	case FrameRenderPassState::Finalized:
		XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] State: Finalized");
		data.handle->finalize(*this, _success);
		break;
	}

	auto it = data.waiters.find(state);
	if (it != data.waiters.end()) {
		for (auto &v : it->second) {
			if (v->state == FrameRenderPassState::Initial) {
				if (isRenderPassReady(*v)) {
					updateRenderPassState(*v, FrameRenderPassState::Ready);
				}
			}
		}
	}

	for (auto &it : data.attachments) {
		if (!it.second->passes.empty() && it.second->passes.back() == &data && it.second->state != FrameAttachmentState::ResourcesReleased) {
			if ((toInt(state) >= toInt(it.second->final))
					|| (toInt(state) >= toInt(FrameRenderPassState::Submitted) && it.second->final == FrameRenderPassState::Initial)) {
				onAttachmentRelease(*it.second);
			}
		}
	}

	if (state >= FrameRenderPassState::Finalized) {
		++ _finalizedObjects;
		tryReleaseFrame();
	}
}

void FrameQueue::onRenderPassReady(FramePassData &data) {
	if (data.handle->isAsync()) {
		updateRenderPassState(data, FrameRenderPassState::Owned);
	} else {
		if (data.handle->getRenderPass()->acquireForFrame(*this, [this, data = &data] (bool success) {
			data->waitForResult = false;
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Owned);
			} else {
				invalidate(*data);
			}
		})) {
			updateRenderPassState(data, FrameRenderPassState::Owned);
		} else {
			data.waitForResult = true;
		}
	}
}

void FrameQueue::onRenderPassOwned(FramePassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	if (data.framebuffer) {
		return;
	}

	Vector<Rc<gl::ImageView>> imageViews;
	bool attachmentsAcquired = true;
	bool _invalidate = false;

	auto acquireView = [&] (const AttachmentPassData *imgDesc, const Rc<ImageStorage> &image) {
		auto imgAttachment = (const ImageAttachment *)imgDesc->attachment->attachment.get();
		gl::ImageViewInfo info = imgAttachment->getImageViewInfo(image->getInfo(), *imgDesc);

		auto view = image->getView(info);
		if (!view) {
			view = image->makeView(info);
		}

		if (view) {
			auto e = view->getExtent();
			if (e.width != data.extent.width || e.height != data.extent.height) {
				XL_FRAME_QUEUE_LOG("Fail to acquire ImageView: invalid extent for RenderPass");
				_invalidate = true;
				attachmentsAcquired = false;
				return;
			}
			imageViews.emplace_back(move(view));
		} else {
			XL_FRAME_QUEUE_LOG("Fail to acquire ImageView for framebuffer");
			_invalidate = true;
			attachmentsAcquired = false;
		}
	};

	data.waitForResult = true;
	for (auto &it : data.attachments) {
		if (it.second->handle->isOutput()) {
			auto out = _frame->getOutputBinding(it.second->handle->getAttachment());
			if (out) {
				_autorelease.emplace_front((FrameOutputBinding *)out);
			}
		}
		if (it.second->state == FrameAttachmentState::Ready) {
			onAttachmentAcquire(*it.second);
			if (it.second->state != FrameAttachmentState::ResourcesAcquired) {
				attachmentsAcquired = false;
				XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] waitForResource: ", it.second->handle->getName());
				waitForResource(*it.second, [this, data = &data] (bool success) {
					if (!success) {
						invalidate();
						return;
					}
					onRenderPassOwned(*data);
				});
			} else {
				if (it.second->image && !it.first->subpasses.empty()) {
					acquireView(it.first, it.second->image);
				}
			}
		} else if (it.second->state == FrameAttachmentState::ResourcesAcquired) {
			if (it.second->image && !it.first->subpasses.empty()) {
				acquireView(it.first, it.second->image);
			}
		}
	}

	if (_invalidate) {
		invalidate();
		return;
	}

	if (attachmentsAcquired) {
		if (!imageViews.empty()) {
			if (data.handle->isFramebufferRequired()) {
				data.framebuffer = _loop->acquireFramebuffer(data.handle->getData(), imageViews, data.extent);
				if (!data.framebuffer) {
					invalidate();
				}
				_autorelease.emplace_front(data.framebuffer);
			}
			if (isResourcePending(data)) {
				XL_FRAME_QUEUE_LOG("[RenderPass:", data.handle->getName(), "] waitForResource (pending): ", data.handle->getName());
				waitForResource(data, [this, data = &data] {
					data->waitForResult = false;
					updateRenderPassState(*data, FrameRenderPassState::ResourcesAcquired);
				});
			} else {
				data.waitForResult = false;
				updateRenderPassState(data, FrameRenderPassState::ResourcesAcquired);
			}
		} else {
			updateRenderPassState(data, FrameRenderPassState::ResourcesAcquired);
		}
	}
}

void FrameQueue::onRenderPassResourcesAcquired(FramePassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	for (auto &it : data.attachments) {
		if (it.second->image) {
			if (auto img = it.second->image->getImage()) {
				data.handle->autorelease(img);
			}
		}
	}

	if (data.framebuffer) {
		data.handle->autorelease(data.framebuffer);
	}

	data.handle->autorelease(_frame->getDevice());

	for (auto &it : data.handle->getData()->subpasses) {
		for (auto &p : it->graphicPipelines) {
			if (p->pipeline) {
				data.handle->autorelease(p->pipeline);
			}
		}
		for (auto &p : it->computePipelines) {
			if (p->pipeline) {
				data.handle->autorelease(p->pipeline);
			}
		}
	}

	if (data.handle->prepare(*this,
			[this, guard = Rc<FrameQueue>(this), data = &data] (bool success) mutable {
		_loop->performOnGlThread([this, data, success] {
			data->waitForResult = false;
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Prepared);
			} else {
				invalidate(*data);
			}
		}, guard, true);
	})) {
		updateRenderPassState(data, FrameRenderPassState::Prepared);
	} else {
		data.waitForResult = true;
	}
}

void FrameQueue::onRenderPassPrepared(FramePassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	if (data.handle->isAsync() || _frame->isReadyForSubmit()) {
		updateRenderPassState(data, FrameRenderPassState::Submission);
	} else {
		_renderPassesPrepared.emplace(&data);
	}
}

void FrameQueue::onRenderPassSubmission(FramePassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	auto sync = makeRenderPassSync(data);

	data.waitForResult = true;
	data.handle->submit(*this, move(sync), [this, guard = Rc<FrameQueue>(this), data = &data] (bool success) {
		_loop->performOnGlThread([this, data, success] {
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Submitted);
			} else {
				data->waitForResult = false;
				invalidate(*data);
			}
		}, guard, true);
	}, [this, guard = Rc<FrameQueue>(this), data = &data] (bool success) {
		_loop->performOnGlThread([this, data, success] {
			data->waitForResult = false;
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Complete);
			} else {
				invalidate(*data);
			}
		}, guard, true);
	});
}

void FrameQueue::onRenderPassSubmitted(FramePassData &data) {
	// no need to check finalization

	++ _renderPassSubmitted;
	if (data.framebuffer) {
		_loop->releaseFramebuffer(move(data.framebuffer));
		data.framebuffer = nullptr;
	}

	if (_renderPassSubmitted == _renderPasses.size()) {
		_frame->onQueueSubmitted(*this);
	}

	for (auto &it : data.attachments) {
		if (it.second->handle->isOutput() && it.first->attachment->attachment->getLastRenderPass() == data.handle->getData()) {
			_frame->onOutputAttachment(*it.second);
		}
	}

	data.handle->getRenderPass()->releaseForFrame(*this);
	if (!data.submitTime) {
		data.submitTime = platform::device::_clock(platform::device::ClockType::Monotonic);
	}
}

void FrameQueue::onRenderPassComplete(FramePassData &data) {
	_submissionTime += platform::device::_clock(platform::device::ClockType::Monotonic) - data.submitTime;
	if (_finalized) {
		invalidate(data);
		return;
	}

	++ _renderPassCompleted;
	if (_renderPassCompleted == _renderPasses.size()) {
		onComplete();
	}
}

Rc<FrameSync> FrameQueue::makeRenderPassSync(FramePassData &data) const {
	auto ret = Rc<FrameSync>::alloc();

	for (auto &it : data.attachments) {
		if (it.first->attachment->attachment->getFirstRenderPass() == data.handle->getData()) {
			if (it.second->image && it.second->image->getWaitSem()) {
				ret->waitAttachments.emplace_back(FrameSyncAttachment{it.second->handle, it.second->image->getWaitSem(),
					it.second->image.get(), getWaitStageForAttachment(data, it.second->handle)});
			}
		}
		if (it.second->handle->getAttachment()->getLastRenderPass() == data.handle->getData()) {
			if (it.second->image && it.second->image->getSignalSem()) {
				ret->signalAttachments.emplace_back(FrameSyncAttachment{it.second->handle, it.second->image->getSignalSem(),
					it.second->image.get()});
			}
		}
		if (it.second->image) {
			auto layout = it.first->finalLayout;
			if (layout == AttachmentLayout::PresentSrc && !it.second->image->isSwapchainImage()) {
				layout = AttachmentLayout::TransferSrcOptimal;
			}
			ret->images.emplace_back(FrameSyncImage{it.second->handle, it.second->image, layout});
		}
	}

	return ret;
}

PipelineStage FrameQueue::getWaitStageForAttachment(FramePassData &data, const AttachmentHandle *handle) const {
	for (auto &it : data.handle->getData()->attachments) {
		if (it->attachment == handle->getAttachment()->getData()) {
			return it->dependency.initialUsageStage;
		}
	}
	return PipelineStage::None;
}

void FrameQueue::onComplete() {
	if (!_finalized) {
		XL_FRAME_QUEUE_LOG("onComplete");
		_success = true;
		_frame->onQueueComplete(*this);
		onFinalized();
	}
}

void FrameQueue::onFinalized() {
	if (_finalized) {
		return;
	}

	XL_FRAME_QUEUE_LOG("onFinalized");

	_finalized = true;
	for (auto &pass : _renderPasses) {
		invalidate(pass.second);
	}

	for (auto &pass : _attachments) {
		invalidate(pass.second);
	}
}

void FrameQueue::invalidate(FrameAttachmentData &data) {
	if (!_finalized) {
		invalidate();
		return;
	}

	if (data.state == FrameAttachmentState::Finalized) {
		return;
	}

	if (!data.waitForResult) {
		finalizeAttachment(data);
	}
}

void FrameQueue::invalidate(FramePassData &data) {
	if (!_finalized) {
		XL_FRAME_QUEUE_LOG("[Queue:", _queue->getName(), "] Invalidated");
		invalidate();
		return;
	}

	if (data.state == FrameRenderPassState::Finalized) {
		return;
	}

	if (data.state == FrameRenderPassState::Ready || data.state == FrameRenderPassState::Owned
			|| (!data.waitForResult && toInt(data.state) > toInt(FrameRenderPassState::Ready))) {
		data.handle->getRenderPass()->releaseForFrame(*this);
		data.waitForResult = false;
	}

	if (!data.waitForResult && data.framebuffer) {
		_loop->releaseFramebuffer(move(data.framebuffer));
		data.framebuffer = nullptr;
	}

	if (!data.waitForResult) {
		updateRenderPassState(data, FrameRenderPassState::Finalized);
	}
}

void FrameQueue::tryReleaseFrame() {
	if (_finalizedObjects == _renderPasses.size() + _attachments.size()) {
		_frame = nullptr;
	}
}

void FrameQueue::finalizeAttachment(FrameAttachmentData &attachment) {
	attachment.handle->finalize(*this, _success);
	XL_FRAME_QUEUE_LOG("[Attachment:", attachment.handle->getName(), "] State: Finalized [", _success, "]");
	attachment.state = FrameAttachmentState::Finalized;
	if (!_success && _frame && attachment.handle->isOutput()) {
		_frame->onOutputAttachmentInvalidated(attachment);
	}
	++ _finalizedObjects;
	tryReleaseFrame();
}

}
