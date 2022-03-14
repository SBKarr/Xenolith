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

#include "XLGlFrameCache.h"
#include "XLGlFrameHandle.h"
#include "XLGlRenderQueue.h"

namespace stappler::xenolith::gl {

bool FrameCacheStorage::init(Device *dev, const FrameEmitter *e, const RenderQueue *q) {
	_device = dev;
	_emitter = e;
	_queue = q;

	_queue->addCacheStorage(this);

	for (auto &it : _queue->getPasses()) {
		_passes.emplace(it, FrameCacheRenderPass{it});
	}
	for (auto &it : _queue->getAttachments()) {
		if (it->getType() == AttachmentType::Image) {
			auto imgAttachment = (ImageAttachment *)it.get();
			_images.emplace(imgAttachment, FrameCacheImageAttachment{imgAttachment});
		}
	}
	return true;
}

void FrameCacheStorage::invalidate() {
	_invalidateMutex.lock();
	_passes.clear();
	_images.clear();
	if (_queue) {
		_queue->removeCacheStorage(this);
		_queue = nullptr;
	}
	if (_emitter) {
		const_cast<FrameEmitter *>(_emitter)->removeCacheStorage(this);
		_emitter = nullptr;
	}
	_invalidateMutex.unlock();
}

void FrameCacheStorage::reset(const RenderPassData *p, Extent2 e) {
	std::unique_lock<Mutex> lock(_invalidateMutex);
	auto it = _passes.find(p);
	if (it == _passes.end()) {
		return;
	}

	if (it->second.extent == e) {
		return;
	}

	if (!it->second.framebuffers.empty()) {
		it->second.framebuffers.clear();
	}
	it->second.extent = e;
}

Rc<Framebuffer> FrameCacheStorage::acquireFramebuffer(const Loop &, const RenderPassData *p, SpanView<Rc<ImageView>> views, Extent2 e) {
	std::unique_lock<Mutex> lock(_invalidateMutex);
	auto passIt = _passes.find(p);
	if (passIt == _passes.end()) {
		return nullptr;
	}

	if (passIt->second.extent != e) {
		if (!passIt->second.framebuffers.empty()) {
			passIt->second.framebuffers.clear();
		}
		passIt->second.extent = e;
	}

	Vector<uint64_t> ids; ids.reserve(views.size());
	for (auto &it : views) {
		ids.emplace_back(it->getIndex());
	}

	auto hash = Framebuffer::getViewHash(ids);

	auto range = passIt->second.framebuffers.equal_range(hash);
	auto it = range.first;
	while (it != range.second) {
		if ((*it).second->getViewIds() == ids) {
			auto ret = move((*it).second);
			passIt->second.framebuffers.erase(it);
			return ret;
		}
		++ it;
	}
	lock.unlock();

	return _device->makeFramebuffer(p, views, passIt->second.extent);
}

void FrameCacheStorage::releaseFramebuffer(const RenderPassData *p, Rc<Framebuffer> &&fb) {
	std::unique_lock<Mutex> lock(_invalidateMutex);
	auto passIt = _passes.find(p);
	if (passIt == _passes.end()) {
		return;
	}

	if (fb->getExtent() != passIt->second.extent) {
		return;
	}

	auto hash = fb->getHash();
	passIt->second.framebuffers.emplace(hash, move(fb));
}

void FrameCacheStorage::reset(const ImageAttachment *a, Extent3 e) {
	std::unique_lock<Mutex> lock(_invalidateMutex);
	auto imageIt = _images.find(a);
	if (imageIt == _images.end()) {
		return;
	}

	if (imageIt->second.extent == e) {
		return;
	}

	if (!imageIt->second.images.empty()) {
		imageIt->second.images.clear();
	}
	imageIt->second.extent = e;
}

Rc<ImageAttachmentObject> FrameCacheStorage::acquireImage(const Loop &loop, const ImageAttachment *a, Extent3 e) {
	std::unique_lock<Mutex> lock(_invalidateMutex);
	auto imageIt = _images.find(a);
	if (imageIt == _images.end()) {
		return nullptr;
	}

	if (imageIt->second.extent != e) {
		if (!imageIt->second.images.empty()) {
			imageIt->second.images.clear();
		}
		imageIt->second.extent = e;
	}

	if (!imageIt->second.images.empty()) {
		auto ret = move(imageIt->second.images.back());
		imageIt->second.images.pop_back();
		ret->rearmSemaphores(*loop.getDevice());
		return ret;
	} else {
		lock.unlock();
		auto ret = _device->makeImage(a, imageIt->second.extent);
		ret->rearmSemaphores(*loop.getDevice());
		return ret;
	}
}

void FrameCacheStorage::releaseImage(const ImageAttachment *a, Rc<ImageAttachmentObject> &&image) {
	if (image->isSwapchainImage) {
		if (image->swapchainImage) {
			image->swapchainImage->cleanup();
		}
		return;
	}

	std::unique_lock<Mutex> lock(_invalidateMutex);
	auto imageIt = _images.find(a);
	if (imageIt == _images.end()) {
		return;
	}

	if (image->extent != imageIt->second.extent) {
		return;
	}

	imageIt->second.images.emplace_back(move(image));
}

Rc<Semaphore> FrameCacheStorage::acquireSemaphore() {
	return _device->makeSemaphore();
}

}
