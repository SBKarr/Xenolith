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

#include "XLRenderQueueFrameCache.h"
#include "XLRenderQueueFrameHandle.h"
#include "XLRenderQueueImageStorage.h"
#include "XLRenderQueueQueue.h"
#include "XLGlDevice.h"
#include "XLGlLoop.h"

namespace stappler::xenolith::renderqueue {

FrameCache::~FrameCache() { }

bool FrameCache::init(gl::Loop &loop, gl::Device &dev) {
	_loop = &loop;
	_device = &dev;
	return true;
}

void FrameCache::invalidate() {
	_framebuffers.clear();
	_imageViews.clear();
	_renderPasses.clear();
	_images.clear();
}

Rc<gl::Framebuffer> FrameCache::acquireFramebuffer(const PassData *data, SpanView<Rc<gl::ImageView>> views, Extent2 e) {
	Vector<uint64_t> ids; ids.reserve(views.size() + 2);
	ids.emplace_back(data->impl->getIndex());
	for (auto &it : views) {
		ids.emplace_back(it->getIndex());
	}
	ids.emplace_back(uint64_t(e.width) << uint64_t(32) | uint64_t(e.height));

	auto it = _framebuffers.find(ids);
	if (it != _framebuffers.end()) {
		if (!it->second.framebuffers.empty()) {
			auto fb = it->second.framebuffers.back();
			it->second.framebuffers.pop_back();
			return fb;
		}
	}

	return _device->makeFramebuffer(data, views, e);
}

void FrameCache::releaseFramebuffer(Rc<gl::Framebuffer> &&fb) {
	auto e = fb->getExtent();
	Vector<uint64_t> ids; ids.reserve(fb->getViewIds().size() + 2);
	ids.emplace_back(fb->getRenderPass()->getIndex());
	for (auto &it : fb->getViewIds()) {
		ids.emplace_back(it);
	}
	ids.emplace_back(uint64_t(e.width) << uint64_t(32) | uint64_t(e.height));

	if (isReachable(SpanView<uint64_t>(ids))) {
		auto it = _framebuffers.find(ids);
		if (it == _framebuffers.end()) {
			_framebuffers.emplace(move(ids), FrameCacheFramebuffer{ Vector<Rc<gl::Framebuffer>>{move(fb)}, e });
		} else {
			it->second.framebuffers.emplace_back(move(fb));
		}
	}
}

Rc<ImageStorage> FrameCache::acquireImage(const gl::ImageInfo &info, SpanView<gl::ImageViewInfo> v) {
	auto imageIt = _images.find(info);
	if (imageIt != _images.end()) {
		if (!imageIt->second.images.empty()) {
			auto ret = move(imageIt->second.images.back());
			imageIt->second.images.pop_back();
			ret->rearmSemaphores(*_loop);
			makeViews(ret, v);
			return ret;
		}
	}

	auto ret = _device->makeImage(info);
	ret->rearmSemaphores(*_loop);
	makeViews(ret, v);
	return ret;
}

void FrameCache::releaseImage(Rc<ImageStorage> &&img) {
	if (!img->isCacheable()) {
		img->cleanup();
		return;
	}

	auto info = img->getInfo();
	auto imageIt = _images.find(info);
	if (imageIt == _images.end()) {
		log::vtext("FrameCache", "releaseImage: cache miss: ", img->getInfo());
		return;
	}

	imageIt->second.images.emplace_back(move(img));
}

void FrameCache::addImage(const ImageInfoData &info) {
	auto it = _images.find(info);
	if (it == _images.end()) {
		_images.emplace(info, FrameCacheImageAttachment{uint32_t(1), Vector<Rc<ImageStorage>>()});
	} else {
		++ it->second.refCount;
	}
}

void FrameCache::removeImage(const ImageInfoData &info) {
	auto it = _images.find(info);
	if (it != _images.end()) {
		if (it->second.refCount == 1) {
			for (auto &iit : it->second.images) {
				_autorelease.emplace_back(iit);
			}
			_images.erase(it);
		} else {
			-- it->second.refCount;
		}
	}
}

void FrameCache::addImageView(uint64_t id) {
	_imageViews.emplace(id);
}

void FrameCache::removeImageView(uint64_t id) {
	auto it = _imageViews.find(id);
	if (it != _imageViews.end()) {
		_imageViews.erase(it);

		auto iit = _framebuffers.begin();
		while (iit != _framebuffers.end()) {
			if (!isReachable(SpanView<uint64_t>(iit->first))) {
				for (auto &it : iit->second.framebuffers) {
					_autorelease.emplace_back(it);
				}
				iit = _framebuffers.erase(iit);
			} else {
				++ iit;
			}
		}
	}
}

void FrameCache::addRenderPass(uint64_t id) {
	_renderPasses.emplace(id);
}

void FrameCache::removeRenderPass(uint64_t id) {
	auto it = _renderPasses.find(id);
	if (it != _renderPasses.end()) {
		_renderPasses.erase(it);

		auto iit = _framebuffers.begin();
		while (iit != _framebuffers.end()) {
			if (!isReachable(SpanView<uint64_t>(iit->first))) {
				for (auto &it : iit->second.framebuffers) {
					_autorelease.emplace_back(it);
				}
				iit = _framebuffers.erase(iit);
			} else {
				++ iit;
			}
		}
	}
}

void FrameCache::removeUnreachableFramebuffers() {
	auto fbsIt = _framebuffers.begin();
	while (fbsIt != _framebuffers.end()) {
		auto e = fbsIt->second.extent;
		bool found = false;
		for (auto &it : _images) {
			if (it.first.extent == Extent3(e, 1)) {
				found = true;
			}
		}
		if (!found) {
			for (auto &it : fbsIt->second.framebuffers) {
				_autorelease.emplace_back(it);
			}
			fbsIt = _framebuffers.erase(fbsIt);
		} else {
			auto fbIt = fbsIt->second.framebuffers.begin();
			while (fbIt != fbsIt->second.framebuffers.end()) {
				auto e = (*fbIt)->getExtent();
				Vector<uint64_t> ids; ids.reserve((*fbIt)->getViewIds().size() + 2);
				ids.emplace_back((*fbIt)->getRenderPass()->getIndex());
				for (auto &it : (*fbIt)->getViewIds()) {
					ids.emplace_back(it);
				}
				ids.emplace_back(uint64_t(e.width) << uint64_t(32) | uint64_t(e.height));

				if (isReachable(SpanView<uint64_t>(ids))) {
					_autorelease.emplace_back(*fbIt);
					fbIt = fbsIt->second.framebuffers.erase(fbIt);
				} else {
					++ fbIt;
				}
			}

			if (fbsIt->second.framebuffers.empty()) {
				fbsIt = _framebuffers.erase(fbsIt);
			} else {
				++ fbsIt;
			}
		}
	}
}

size_t FrameCache::getFramebuffersCount() const {
	size_t ret = 0;
	for (auto &it : _framebuffers) {
		ret += it.second.framebuffers.size();
	}
	return ret;
}

size_t FrameCache::getImagesCount() const {
	size_t ret = 0;
	for (auto &it : _images) {
		ret += it.second.images.size();
	}
	return ret;
}

size_t FrameCache::getImageViewsCount() const {
	return _imageViews.size();
}

void FrameCache::clear() {
	if (!_freezed) {
		_autorelease.clear();
	}
}

void FrameCache::freeze() {
	_freezed = true;
}

void FrameCache::unfreeze() {
	if (_freezed) {
		_autorelease.clear();
	}
	_freezed = false;
}

bool FrameCache::isReachable(SpanView<uint64_t> ids) const {
	auto fb = ids.front();
	if (_renderPasses.find(fb) == _renderPasses.end()) {
		return false;
	}

	ids = ids.sub(1, ids.size() - 2);
	for (auto &it : ids) {
		if (_imageViews.find(it) == _imageViews.end()) {
			return false;
		}
	}
	return true;
}

bool FrameCache::isReachable(const ImageInfoData &info) const {
	auto it = _images.find(info);
	return it != _images.end();
}

void FrameCache::makeViews(const Rc<ImageStorage> &img, SpanView<gl::ImageViewInfo> views) {
	for (auto &info : views) {
		auto v = img->getView(info);
		if (!v) {
			auto v = _device->makeImageView(img->getImage(), info);
			addImageView(v->getIndex());
			v->setReleaseCallback([loop = Rc<gl::Loop>(_loop), id = v->getIndex()] {
				loop->performOnGlThread([loop, id] {
					loop->getFrameCache()->removeImageView(id);
				});
			});
			img->addView(info, move(v));
		}
	}
}

}
