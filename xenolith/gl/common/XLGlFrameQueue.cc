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

#include "XLGlFrameQueue.h"
#include "XLGlFrameHandle.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::gl {

FrameQueue::~FrameQueue() {
	_frame = nullptr;
}

bool FrameQueue::init(const Rc<PoolRef> &p, const Rc<RenderQueue> &q, const Rc<FrameCacheStorage> &cache, FrameHandle &f, Extent2 ext) {
	_pool = p;
	_queue = q;
	_cache = cache;
	_frame = &f;
	_loop = _frame->getLoop();
	_extent = ext;
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
			auto v = _renderPasses.emplace(it, FrameQueueRenderPassData{
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
		if (it->getType() == AttachmentType::Image) {
			auto img = (ImageAttachment *)it.get();
			extent = img->getSizeForFrame(*this);
		}
		auto h = it->makeFrameHandle(*this);
		if (h->isAvailable(*this)) {
			auto v = _attachments.emplace(it, FrameQueueAttachmentData({
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
				log::vtext("gl::FrameQueue", "RenderPass '", pass->key, "' is not available on frame");
				valid = false;
			}
		}

		auto &descs = it.second.handle->getAttachment()->getDescriptors();
		auto &last = descs.back();
		it.second.final = last->getRequiredRenderPassState();
	}

	for (auto &passIt : _renderPasses) {
		for (auto &a : passIt.first->descriptors) {
			auto aIt = _attachments.find(a->getAttachment());
			if (aIt != _attachments.end()) {
				passIt.second.attachments.emplace_back(a, &aIt->second);
			} else {
				log::vtext("gl::FrameQueue", "Attachment '", a->getName(), "' is not available on frame");
				valid = false;
			}
		}

		for (auto &a : passIt.second.attachments) {
			auto &desc = a.first->getAttachment()->getDescriptors();
			auto it = desc.begin();
			while (it != desc.end() && (*it)->getRenderPass() != passIt.second.handle->getData()) {
				auto iit = _renderPasses.find((*it)->getRenderPass());
				if (iit != _renderPasses.end()) {
					addRequiredPass(passIt.second, iit->second, *a.second, **it);
				} else {
					log::vtext("gl::FrameQueue", "RenderPass '", (*it)->getRenderPass()->key, "' is not available on frame");
					valid = false;
				}
			}

			passIt.second.attachmentMap.emplace(a.first->getAttachment(), a.second);
		}
	}

	for (auto &passIt : _renderPasses) {
		for (auto &it : passIt.second.required) {
			auto wIt = it.first->waiters.find(it.second);
			if (wIt == it.first->waiters.end()) {
				wIt = it.first->waiters.emplace(it.second, Vector<FrameQueueRenderPassData *>()).first;
			}
			wIt->second.emplace_back(&passIt.second);
		}
	}

	return valid;
}

void FrameQueue::update() {
	XL_FRAME_LOG("[", _loop->getClock(), "] [", _frame->getOrder(), "] [", FrameHandle::GetActiveFramesCount(), "] update");

	for (auto &it : _attachmentsInitial) {
		if (it->handle->setup(*this, [this, guard = Rc<FrameQueue>(this), attachment = it] (bool success) {
			_loop->performOnThread([this, attachment, success] {
				attachment->waitForResult = false;
				if (success && !_finalized) {
					onAttachmentSetupComplete(*attachment);
					_loop->pushContextEvent(Loop::EventName::FrameUpdate, _frame);
				} else {
					invalidate(*attachment);
				}
			}, guard, true);
		})) {
			onAttachmentSetupComplete(*it);
		} else {
			it->waitForResult = true;
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
		auto it = _renderPassesPrepared.begin();
		while (it != _renderPassesPrepared.end()) {
			if ((*it)->state == FrameRenderPassState::Prepared) {
				auto v = *it;
				onRenderPassPrepared(*v);
				if (v->state != FrameRenderPassState::Prepared) {
					it = _renderPassesInitial.erase(it);
				} else {
					++ it;
				}
			} else {
				it = _renderPassesInitial.erase(it);
			}
		}
	} while(0);
}

void FrameQueue::invalidate() {
	if (!_finalized) {
		_success = false;
		auto f = _frame;
		onFinalized();
		f->onQueueInvalidated(*this);
	}
}

Loop *FrameQueue::getLoop() const {
	return _loop;
}

const FrameQueueAttachmentData *FrameQueue::getAttachment(const Attachment *a) const {
	auto it = _attachments.find(a);
	if (it != _attachments.end()) {
		return &it->second;
	}
	return nullptr;
}

const FrameQueueRenderPassData *FrameQueue::getRenderPass(const RenderPassData *p) const {
	auto it = _renderPasses.find(p);
	if (it != _renderPasses.end()) {
		return &it->second;
	}
	return nullptr;
}

void FrameQueue::addRequiredPass(FrameQueueRenderPassData &pass, const FrameQueueRenderPassData &required,
		const FrameQueueAttachmentData &attachment, const AttachmentDescriptor &desc) {
	if (desc.getRequiredRenderPassState() == FrameRenderPassState::Initial) {
		return;
	}

	auto lb = std::lower_bound(pass.required.begin(), pass.required.end(), &required,
			[&] (const Pair<FrameQueueRenderPassData *, FrameRenderPassState> &l, const FrameQueueRenderPassData *r) {
		return l.first < r;
	});
	if (lb == pass.required.end()) {
		pass.required.emplace_back((FrameQueueRenderPassData *)&required, desc.getRequiredRenderPassState());
	} else if (lb->first != &required) {
		pass.required.emplace(lb, (FrameQueueRenderPassData *)&required, desc.getRequiredRenderPassState());
	} else {
		lb->second = FrameRenderPassState(std::max(toInt(lb->second), toInt(desc.getRequiredRenderPassState())));
	}
}

bool FrameQueue::isResourcePending(const FrameQueueAttachmentData &image) {
	return false;
}

void FrameQueue::waitForResource(const FrameQueueAttachmentData &image, Function<void()> &&cb) {
	// TODO
}

bool FrameQueue::isResourcePending(const FrameQueueRenderPassData &) {
	return false;
}

void FrameQueue::waitForResource(const FrameQueueRenderPassData &, Function<void()> &&) {
	// TODO
}

void FrameQueue::onAttachmentSetupComplete(FrameQueueAttachmentData &attachment) {
	if (attachment.handle->isOutput()) {
		// do nothing for now
	}
	if (attachment.handle->isInput()) {
		attachment.state = FrameAttachmentState::InputRequired;
		if (auto data = _frame->getInputData(attachment.handle->getAttachment().get())) {
			attachment.waitForResult = true;
			attachment.handle->submitInput(*this, move(data),
					[this, guard = Rc<FrameQueue>(this), attachment = &attachment] (bool success) {
				_loop->performOnThread([this, attachment, success] {
					attachment->waitForResult = false;
					if (success && !_finalized) {
						onAttachmentInput(*attachment);
						_loop->pushContextEvent(Loop::EventName::FrameUpdate, _frame);
					} else {
						invalidate(*attachment);
					}
				}, guard, true);
			});
		} else {
			attachment.waitForResult = true;
			attachment.handle->getAttachment()->acquireInput(*this, attachment.handle,
					[this, guard = Rc<FrameQueue>(this), attachment = &attachment] (bool success) {
				_loop->performOnThread([this, attachment, success] {
					attachment->waitForResult = false;
					if (success && !_finalized) {
						onAttachmentInput(*attachment);
						_loop->pushContextEvent(Loop::EventName::FrameUpdate, _frame);
					} else {
						invalidate(*attachment);
					}
				}, guard, true);
			});
		}
	} else {
		attachment.state = FrameAttachmentState::Ready;
	}
}

void FrameQueue::onAttachmentInput(FrameQueueAttachmentData &attachment) {
	attachment.state = FrameAttachmentState::Ready;
}

void FrameQueue::onAttachmentAcquire(FrameQueueAttachmentData &attachment) {
	if (_finalized) {
		if (attachment.state != FrameAttachmentState::Finalized) {
			attachment.handle->finalize(*this, _success);
			attachment.state = FrameAttachmentState::Finalized;
			++ _finalizedObjects;
			tryReleaseFrame();
		}
		return;
	}

	attachment.state = FrameAttachmentState::ResourcesPending;
	if (attachment.handle->getAttachment()->getType() == AttachmentType::Image) {
		auto img = (ImageAttachment *)attachment.handle->getAttachment().get();

		if constexpr (config::EnableSwapchainHook) {
			if (_frame->isSwapchainAttachment(attachment.handle->getAttachment())) {
				attachment.image = _frame->acquireSwapchainImage(*_loop, img, attachment.extent);
			}
		}

		if (!attachment.image) {
			attachment.image = _cache->acquireImage(*_loop, img, attachment.extent);
		}

		_autorelease.emplace_front(attachment.image);
		if (attachment.image->signalSem) {
			_autorelease.emplace_front(attachment.image->signalSem);
		}
		if (attachment.image->waitSem) {
			_autorelease.emplace_front(attachment.image->waitSem);
		}

		if (isResourcePending(attachment)) {
			waitForResource(attachment, [attachment = &attachment] {
				attachment->state = FrameAttachmentState::ResourcesAcquired;
			});
		} else {
			attachment.state = FrameAttachmentState::ResourcesAcquired;
		}
	} else {
		attachment.state = FrameAttachmentState::ResourcesAcquired;
	}
}

void FrameQueue::onAttachmentRelease(FrameQueueAttachmentData &attachment) {
	if (attachment.image) {
		if (attachment.handle->getAttachment()->getType() == AttachmentType::Image) {
			auto img = (ImageAttachment *)attachment.handle->getAttachment().get();
			_cache->releaseImage(img, move(attachment.image));
			attachment.image = nullptr;
		}

		if (_finalized && attachment.state != FrameAttachmentState::Finalized) {
			attachment.handle->finalize(*this, _success);
			attachment.state = FrameAttachmentState::Finalized;
			++ _finalizedObjects;
			tryReleaseFrame();
		} else {
			attachment.state = FrameAttachmentState::ResourcesReleased;
		}
	} else {
		if (_finalized && attachment.state != FrameAttachmentState::Finalized) {
			attachment.handle->finalize(*this, _success);
			attachment.state = FrameAttachmentState::Finalized;
			++ _finalizedObjects;
			tryReleaseFrame();
		} else {
			attachment.state = FrameAttachmentState::ResourcesReleased;
		}
	}

}

bool FrameQueue::isRenderPassReady(const FrameQueueRenderPassData &data) const {
	for (auto &it : data.required) {
		if (toInt(it.first->state) < toInt(it.second)) {
			return false;
		}
	}

	for (auto &it : data.attachments) {
		if (toInt(it.second->state) < toInt(FrameAttachmentState::Ready)) {
			return false;
		}
	}
	return true;
}

void FrameQueue::updateRenderPassState(FrameQueueRenderPassData &data, FrameRenderPassState state) {
	if (state == FrameRenderPassState::Ready && data.handle->isAsync()) {
		state = FrameRenderPassState::Owned;
	}

	if (toInt(data.state) >= toInt(state)) {
		return;
	}

	data.state = state;

	switch (state) {
	case FrameRenderPassState::Initial: break;
	case FrameRenderPassState::Ready:
		onRenderPassReady(data);
		break;
	case FrameRenderPassState::Owned:
		onRenderPassOwned(data);
		break;
	case FrameRenderPassState::ResourcesAcquired:
		onRenderPassResourcesAcquired(data);
		break;
	case FrameRenderPassState::Prepared:
		onRenderPassPrepared(data);
		break;
	case FrameRenderPassState::Submission:
		onRenderPassSubmission(data);
		break;
	case FrameRenderPassState::Submitted:
		onRenderPassSubmitted(data);
		break;
	case FrameRenderPassState::Complete:
		onRenderPassComplete(data);
		break;
	case FrameRenderPassState::Finalized:
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
		if (it.second->passes.back() == &data && it.second->state != FrameAttachmentState::ResourcesReleased) {
			if ((toInt(state) >= toInt(it.second->final))
					|| (toInt(state) >= toInt(FrameRenderPassState::Submitted) && it.second->final == FrameRenderPassState::Initial)) {
				onAttachmentRelease(*it.second);
			}
		}
	}

	if (state == FrameRenderPassState::Finalized) {
		++ _finalizedObjects;
		tryReleaseFrame();
	}
}

void FrameQueue::onRenderPassReady(FrameQueueRenderPassData &data) {
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

void FrameQueue::onRenderPassOwned(FrameQueueRenderPassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	if (data.framebuffer) {
		return;
	}

	Vector<Rc<ImageView>> imageViews;
	bool attachmentsAcquired = true;

	data.waitForResult = true;
	for (auto &it : data.attachments) {
		if (it.second->state == FrameAttachmentState::Ready) {
			onAttachmentAcquire(*it.second);
			if (it.second->state != FrameAttachmentState::ResourcesAcquired) {
				attachmentsAcquired = false;
				waitForResource(*it.second, [this, data = &data] {
					onRenderPassOwned(*data);
				});
			} else {
				if (it.second->image) {
					auto imgDesc = (gl::ImageAttachmentDescriptor *)it.first;
					ImageViewInfo info(*imgDesc);

					auto vIt = it.second->image->views.find(info);
					if (vIt != it.second->image->views.end()) {
						imageViews.emplace_back(vIt->second);
					}
				}
			}
		}
	}

	if (attachmentsAcquired) {
		if (!imageViews.empty()) {
			data.framebuffer = _cache->acquireFramebuffer(*_loop, data.handle->getData(), imageViews, data.extent);
			_autorelease.emplace_front(data.framebuffer);
			if (isResourcePending(data)) {
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

void FrameQueue::onRenderPassResourcesAcquired(FrameQueueRenderPassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	if (data.handle->prepare(*this,
			[this, guard = Rc<FrameQueue>(this), data = &data] (bool success) {
		_loop->performOnThread([this, data, success] {
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

void FrameQueue::onRenderPassPrepared(FrameQueueRenderPassData &data) {
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

void FrameQueue::onRenderPassSubmission(FrameQueueRenderPassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	auto sync = makeRenderPassSync(data);

	data.waitForResult = true;
	data.handle->submit(*this, move(sync), [this, guard = Rc<FrameQueue>(this), data = &data] (bool success) {
		_loop->performOnThread([this, data, success] {
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Submitted);
			} else {
				data->waitForResult = false;
				invalidate(*data);
			}
		}, guard, true);
	}, [this, guard = Rc<FrameQueue>(this), data = &data] (bool success) {
		_loop->performOnThread([this, data, success] {
			data->waitForResult = false;
			if (success && !_finalized) {
				updateRenderPassState(*data, FrameRenderPassState::Complete);
			} else {
				invalidate(*data);
			}
		}, guard, true);
	});
}

void FrameQueue::onRenderPassSubmitted(FrameQueueRenderPassData &data) {
	// no need to check finalization

	++ _renderPassSubmitted;
	if (data.framebuffer) {
		_cache->releaseFramebuffer(data.handle->getData(), move(data.framebuffer));
		data.framebuffer = nullptr;
	}

	for (auto &it : data.attachments) {
		if (it.second->handle->isOutput() && it.first->getAttachment()->getLastRenderPass() == data.handle->getData()) {
			_frame->onOutputAttachment(*it.second);
		}
	}

	data.handle->getRenderPass()->releaseForFrame(*this);

	if (_renderPassSubmitted == _renderPasses.size()) {
		_frame->onQueueSubmitted(*this);
	}
}

void FrameQueue::onRenderPassComplete(FrameQueueRenderPassData &data) {
	if (_finalized) {
		invalidate(data);
		return;
	}

	++ _renderPassCompleted;
	if (_renderPassCompleted == _renderPasses.size()) {
		onComplete();
	}
}

Rc<FrameSync> FrameQueue::makeRenderPassSync(FrameQueueRenderPassData &data) const {
	auto ret = Rc<FrameSync>::alloc();

	for (auto &it : data.attachments) {
		if (it.first->getAttachment()->getFirstRenderPass() == data.handle->getData()) {
			if (it.second->image && it.second->image->waitSem) {
				ret->waitAttachments.emplace_back(FrameSyncAttachment{it.second->handle, it.second->image->waitSem,
					getWaitStageForAttachment(data, it.second->handle)});
			}
		}
		if (it.second->handle->getAttachment()->getLastRenderPass() == data.handle->getData()) {
			if (it.second->image && it.second->image->signalSem) {
				ret->signalAttachments.emplace_back(FrameSyncAttachment{it.second->handle, it.second->image->signalSem});
			}
		}
		if (auto desc = data.handle->getRenderPass()->getDescriptor(it.second->handle->getAttachment())) {
			if (it.second->image) {
				auto imgDesc = (ImageAttachmentDescriptor *)desc;
				auto layout = imgDesc->getFinalLayout();
				if (layout == gl::AttachmentLayout::PresentSrc && !it.second->image->isSwapchainImage) {
					layout = gl::AttachmentLayout::TransferSrcOptimal;
				}
				ret->images.emplace_back(FrameSyncImage{it.second->handle, it.second->image, layout});
			}
		}
	}

	return ret;
}

PipelineStage FrameQueue::getWaitStageForAttachment(FrameQueueRenderPassData &data, const gl::AttachmentHandle *handle) const {
	for (auto &it : data.handle->getData()->descriptors) {
		if (it->getAttachment() == handle->getAttachment()) {
			return it->getDependency().initialUsageStage;
		}
	}
	return PipelineStage::None;
}

void FrameQueue::onComplete() {
	if (!_finalized) {
		_success = true;
		_frame->onQueueComplete(*this);
		onFinalized();
	}
}

void FrameQueue::onFinalized() {
	if (_finalized) {
		return;
	}

	_finalized = true;
	for (auto &pass : _renderPasses) {
		invalidate(pass.second);
	}

	for (auto &pass : _attachments) {
		invalidate(pass.second);
	}
}

void FrameQueue::invalidate(FrameQueueAttachmentData &data) {
	if (!_finalized) {
		invalidate();
		return;
	}

	if (data.state == FrameAttachmentState::Finalized) {
		return;
	}

	if (!data.waitForResult) {
		data.handle->finalize(*this, _success);
		data.state = FrameAttachmentState::Finalized;
		++ _finalizedObjects;
		tryReleaseFrame();
	}
}

void FrameQueue::invalidate(FrameQueueRenderPassData &data) {
	if (!_finalized) {
		invalidate();
		return;
	}

	if (data.state == FrameRenderPassState::Finalized) {
		return;
	}

	if (data.state == FrameRenderPassState::Ready || (!data.waitForResult && toInt(data.state) > toInt(FrameRenderPassState::Ready))) {
		data.handle->getRenderPass()->releaseForFrame(*this);
	}

	if (!data.waitForResult && data.framebuffer) {
		_cache->releaseFramebuffer(data.handle->getData(), move(data.framebuffer));
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

}
