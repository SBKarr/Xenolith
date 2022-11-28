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

#include "XLVkView.h"
#include "XLGlLoop.h"
#include "XLVkDevice.h"
#include "XLDirector.h"
#include "XLRenderQueueImageStorage.h"

namespace stappler::xenolith::vk {

void View_writeImageTransfer(Device &dev, VkCommandBuffer buf, const Rc<Image> &sourceImage, const Rc<Image> &targetImage,
		VkImageLayout sourceLayout, VkFilter filter) {
	auto table = dev.getTable();

	VkCommandBufferBeginInfo beginInfo { };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (table->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		return;
	}

	auto sourceExtent = sourceImage->getInfo().extent;
	auto targetExtent = targetImage->getInfo().extent;

	Vector<VkImageMemoryBarrier> inputImageBarriers;
	inputImageBarriers.emplace_back(VkImageMemoryBarrier{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
		VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		targetImage->getImage(), VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
	});

	Vector<VkImageMemoryBarrier> outputImageBarriers;
	outputImageBarriers.emplace_back(VkImageMemoryBarrier{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		targetImage->getImage(), VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
	});

	if (sourceLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		inputImageBarriers.emplace_back(VkImageMemoryBarrier{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
			sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			targetImage->getImage(), VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
		});
	}

	if (!inputImageBarriers.empty()) {
		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			inputImageBarriers.size(), inputImageBarriers.data());
	}

	if (sourceExtent == targetExtent) {
		VkImageCopy copy{
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
			VkOffset3D{0, 0, 0},
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
			VkOffset3D{0, 0, 0},
			VkExtent3D{targetExtent.width, targetExtent.height, targetExtent.depth}
		};

		table->vkCmdCopyImage(buf, sourceImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				targetImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	} else {
		VkImageBlit blit{
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
			{ VkOffset3D{0, 0, 0}, VkOffset3D{int32_t(sourceExtent.width), int32_t(sourceExtent.height), int32_t(sourceExtent.depth)} },
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
			{ VkOffset3D{0, 0, 0}, VkOffset3D{int32_t(targetExtent.width), int32_t(targetExtent.height), int32_t(targetExtent.depth)} },
		};

		table->vkCmdBlitImage(buf, sourceImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				targetImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);
	}

	if (!outputImageBarriers.empty()) {
		table->vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
			0, nullptr,
			0, nullptr,
			outputImageBarriers.size(), outputImageBarriers.data());
	}

	table->vkEndCommandBuffer(buf);
}

View::~View() {
	//_thread.join();
}

bool View::init(Loop &loop, Device &dev, gl::ViewInfo &&info) {
	if (!gl::View::init(loop, move(info))) {
		return false;
	}

	_threadName = toString("View:", info.name);
	_instance = (Instance *)loop.getGlInstance().get();
	_device = &dev;
	_director = Rc<Director>::create(_loop->getApplication(), this);
	if (_onCreated) {
		_loop->getApplication()->performOnMainThread([this] {
			_onCreated(_director);
		}, this);
	} else {
		run();
	}
	return true;
}

void View::threadInit() {
	_running = true;
	_avgFrameInterval.reset(0);

	retain();
	thread::ThreadInfo::setThreadInfo(_threadName);
	_threadId = std::this_thread::get_id();
	_shouldQuit.test_and_set();

	auto info = getSurfaceOptions();

	auto cfg = _selectConfig(info);

	if (info.surfaceDensity != 1.0f) {
		_density = _loop->getApplication()->getData().density * info.surfaceDensity;
	}

	createSwapchain(move(cfg), cfg.presentMode);

	if (_initImage) {
		presentImmediate(move(_initImage), nullptr);
		_initImage = nullptr;
	}

	mapWindow();

	scheduleNextImage(_frameInterval, false);
}

void View::threadDispose() {
	_mutex.lock();
	_running = false;

	auto loop = (Loop *)_loop.get();
	for (auto &it : _fences) {
		it->check(*loop, false);
	}
	_fences.clear();
	_mutex.unlock();

	for (auto &it :_fenceImages) {
		it->invalidateSwapchain();
	}
	_fenceImages.clear();

	for (auto &it :_scheduledImages) {
		it->invalidateSwapchain();
	}
	_scheduledImages.clear();

	for (auto &it :_scheduledPresent) {
		it->invalidateSwapchain();
	}
	_scheduledPresent.clear();

	//_swapchain->invalidate();
	_swapchain = nullptr;
	_surface = nullptr;

	finalize();
	release(0);
}

void View::update(bool displayLink) {
	gl::View::update(displayLink);

	updateFences();

	if (displayLink && _options.followDisplayLink) {
		// ignore present windows
		for (auto &it : _scheduledPresent) {
			runScheduledPresent(move(it));
		}
		_scheduledPresent.clear();
	}

	do {
		auto it = _fenceImages.begin();
		while (it != _fenceImages.end()) {
			if (_fenceOrder < (*it)->getOrder()) {
				_scheduledImages.emplace_back(move(*it));
				it = _fenceImages.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	do {
		auto it = _scheduledImages.begin();
		while (it != _scheduledImages.end()) {
			if (acquireScheduledImage((*it), AcquireSwapchainImageAsync)) {
				it = _scheduledImages.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	if (!_options.followDisplayLink) {
		if (_scheduledPresent.empty()) {
			return;
		}

		auto clock = platform::device::_clock(platform::device::ClockType::Monotonic);

		do {
			auto it = _scheduledPresent.begin();
			while (it != _scheduledPresent.end()) {
				if (!(*it)->getPresentWindow() || (*it)->getPresentWindow() < clock) {
					runScheduledPresent(move(*it));
					it = _scheduledPresent.erase(it);
				} else {
					++ it;
				}
			}
		} while (0);
	}
}

void View::run() {
	_threadStarted = true;
	_thread = std::thread(View::workerThread, this, nullptr);
}

void View::runWithQueue(const Rc<RenderQueue> &queue) {
	auto req = Rc<FrameRequest>::create(queue, _frameEmitter, _screenExtent, _density);
	req->bindSwapchainCallback([this] (renderqueue::FrameAttachmentData &attachment, bool success) {
		if (success) {
			_initImage = move(attachment.image);
		}
		run();
		return true;
	});

	_frameEmitter->submitNextFrame(move(req));
}

void View::onAdded(Device &dev) {
	std::unique_lock<Mutex> lock(_mutex);
	_device = &dev;
	_running = true;
}

void View::onRemoved() {
	std::unique_lock<Mutex> lock(_mutex);
	_running = false;
	_callbacks.clear();
	lock.unlock();
	if (_threadStarted) {
		_thread.join();
	}
}

void View::deprecateSwapchain(bool fast) {
	performOnThread([this, fast] {
		_swapchain->deprecate(fast);

		auto it = _scheduledPresent.begin();
		while (it != _scheduledPresent.end()) {
			runScheduledPresent(move(*it));
			it = _scheduledPresent.erase(it);
		}

		if (!_blockDeprecation && _swapchain->getAcquiredImagesCount() == 0) {
			recreateSwapchain(_swapchain->getRebuildMode());
		}
	}, this, true);
}

bool View::present(Rc<ImageStorage> &&object) {
	if (object->isSwapchainImage()) {
		if (_options.followDisplayLink) {
			performOnThread([this, object = move(object)] () mutable {
				auto img = (SwapchainImage *)object.get();
				_scheduledPresent.emplace_back(img);
			}, this);
			return false;
		}
		auto img = (SwapchainImage *)object.get();
		if (!img->getPresentWindow() || img->getPresentWindow() < platform::device::_clock(platform::device::ClockType::Monotonic)) {
			auto queue = _device->tryAcquireQueueSync(QueueOperations::Present, false);
			if (queue) {
				performOnThread([this, queue = move(queue), object = move(object)] () mutable {
					auto img = (SwapchainImage *)object.get();
					if (img->getSwapchain() == _swapchain && img->isSubmitted()) {
						presentWithQueue(*queue, move(object));
					}
					_loop->performOnGlThread([this,  queue = move(queue)] () mutable {
						_device->releaseQueue(move(queue));
					}, this);
				}, this);
			} else {
				_device->acquireQueue(QueueOperations::Present, *(Loop *)_loop.get(),
						[this, object = move(object)] (Loop &, const Rc<DeviceQueue> &queue) mutable {
					performOnThread([this, queue, object = move(object)] () mutable {
						auto img = (SwapchainImage *)object.get();
						if (img->getSwapchain() == _swapchain && img->isSubmitted()) {
							presentWithQueue(*queue, move(object));
						}
						_loop->performOnGlThread([this,  queue = move(queue)] () mutable {
							_device->releaseQueue(move(queue));
						}, this);
					}, this);
				}, [this] (Loop &) {
					invalidate();
				}, this);
			}
		} else {
			performOnThread([this, object = move(object)] () mutable {
				auto img = (SwapchainImage *)object.get();
				_scheduledPresent.emplace_back(img);
			}, this, true);
		}
	} else {
		if (!_options.renderImageOffscreen) {
			return true;
		}
		auto gen = _gen;
		performOnThread([this, object = move(object), gen] () mutable {
			presentImmediate(move(object), [this, gen] (bool success) {
				if (gen == _gen) {
					scheduleNextImage(0, false);
				}
			});
			if (_swapchain->isDeprecated()) {
				recreateSwapchain(_swapchain->getRebuildMode());
			}
		}, this);
		return true;
	}
	return false;
}

bool View::presentImmediate(Rc<ImageStorage> &&object, Function<void(bool)> &&scheduleCb) {
	auto ops = QueueOperations::Present;
	auto dev = (Device *)_device.get();

	VkFilter filter;
	if (!isImagePresentable(*object->getImage(), filter)) {
		return false;
	}

	Rc<DeviceQueue> queue;
	Rc<CommandPool> pool;
	Rc<Fence> presentFence;

	Rc<Image> sourceImage = (Image *)object->getImage().get();
	Rc<ImageStorage> targetImage;

	Vector<VkCommandBuffer> buffers;
	Loop *loop = (Loop *)_loop.get();

	auto cleanup = [&] {
		if (presentFence) {
			presentFence = nullptr;
		}
		if (pool) {
			dev->releaseCommandPoolUnsafe(move(pool));
			pool = nullptr;
		}
		if (queue) {
			dev->releaseQueue(move(queue));
			queue = nullptr;
		}
		return false;
	};

#if XL_VKAPI_DEBUG
	auto t = platform::device::_clock(platform::device::ClockType::Monotonic);
#endif

	if (_options.waitOnSwapchainPassFence) {
		waitForFences(_frameOrder);
	}

	XL_VKAPI_LOG("[PresentImmediate] [waitForFences] [", platform::device::_clock(platform::device::Monotonic) - t, "]");

	if (!scheduleCb) {
		presentFence = loop->acquireFence(0, false);
	}

	targetImage = _swapchain->acquire(true, presentFence);
	if (!targetImage) {
		XL_VKAPI_LOG("[PresentImmediate] [acquire-failed] [", platform::device::_clock(platform::device::Monotonic) - t, "]");
		if (presentFence) {
			presentFence->schedule(*loop);
		}
		return cleanup();
	}

	XL_VKAPI_LOG("[PresentImmediate] [acquire] [", platform::device::_clock(platform::device::Monotonic) - t, "]");

	pool = dev->acquireCommandPool(ops);

	auto buf = pool->allocBuffer(*dev);

	View_writeImageTransfer(*dev, buf, sourceImage, targetImage->getImage().cast<Image>(), VkImageLayout(object->getLayout()), filter);

	buffers.emplace_back(buf);

	renderqueue::FrameSync frameSync;
	object->rearmSemaphores(*loop);

	frameSync.waitAttachments.emplace_back(renderqueue::FrameSyncAttachment{nullptr, object->getWaitSem(),
		object.get(), renderqueue::PipelineStage::Transfer});
	frameSync.waitAttachments.emplace_back(renderqueue::FrameSyncAttachment{nullptr, targetImage->getWaitSem(),
		targetImage.get(), renderqueue::PipelineStage::Transfer});

	frameSync.signalAttachments.emplace_back(renderqueue::FrameSyncAttachment{nullptr, targetImage->getSignalSem(),
		targetImage.get()});

	XL_VKAPI_LOG("[PresentImmediate] [writeBuffers] [", platform::device::_clock(platform::device::Monotonic) - t, "]");

	if (presentFence) {
		presentFence->check(*(Loop *)_loop.get(), false);
	}

	XL_VKAPI_LOG("[PresentImmediate] [acquireFence] [", platform::device::_clock(platform::device::Monotonic) - t, "]");

	queue = dev->tryAcquireQueueSync(ops, true);
	if (!queue) {
		return cleanup();
	}

	XL_VKAPI_LOG("[PresentImmediate] [acquireQueue] [", platform::device::_clock(platform::device::Monotonic) - t, "]");

	if (!presentFence) {
		presentFence = loop->acquireFence(0, false);
	}

	if (!queue->submit(frameSync, *presentFence, *pool, buffers)) {
		return cleanup();
	}

	XL_VKAPI_LOG("[PresentImmediate] [submit] [", platform::device::_clock(platform::device::Monotonic) - t, "]");

	auto result = _swapchain->present(*queue, targetImage);
	updateFrameInterval();

	XL_VKAPI_LOG("[PresentImmediate] [present] [", platform::device::_clock(platform::device::Monotonic) - t, "]");

	if (result == VK_SUCCESS) {
		if (queue) {
			dev->releaseQueue(move(queue));
			queue = nullptr;
		}
		if (scheduleCb) {
			pool->autorelease(object);
			presentFence->addRelease([dev, pool = pool ? move(pool) : nullptr, scheduleCb = move(scheduleCb), object = move(object), loop] (bool success) mutable {
				if (pool) {
					dev->releaseCommandPoolUnsafe(move(pool));
				}
				loop->releaseImage(move(object));
				scheduleCb(success);
			}, this, "View::presentImmediate::releaseCommandPoolUnsafe");
			scheduleFence(move(presentFence));
		} else {
			presentFence->check(*((Loop *)_loop.get()), false);
			dev->releaseCommandPoolUnsafe(move(pool));
			loop->releaseImage(move(object));
		}
		XL_VKAPI_LOG("[PresentImmediate] [presentFence] [", platform::device::_clock(platform::device::Monotonic) - t, "]");
		presentFence = nullptr;
		XL_VKAPI_LOG("[PresentImmediate] [", platform::device::_clock(platform::device::Monotonic) - t, "]");
		return true;
	} else {
		if (queue) {
			queue->waitIdle();
			dev->releaseQueue(move(queue));
			queue = nullptr;
		}
		if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
			_swapchain->deprecate(false);
			presentFence->check(*loop, false);
			XL_VKAPI_LOG("[PresentImmediate] [presentFence] [", platform::device::_clock(platform::device::Monotonic) - t, "]");
			presentFence = nullptr;

			dev->releaseCommandPoolUnsafe(move(pool));
			pool = nullptr;
		}
		XL_VKAPI_LOG("[PresentImmediate] [", platform::device::_clock(platform::device::Monotonic) - t, "]");
		return cleanup();
	}
}

void View::invalidateTarget(Rc<ImageStorage> &&object) {
	if (object->isSwapchainImage()) {
		auto img = (SwapchainImage *)object.get();
		img->invalidateImage();
	} else {

	}
}

Rc<Ref> View::getSwapchainHandle() const {
	return _swapchain.get();
}

void View::captureImage(StringView name, const Rc<gl::ImageObject> &image, AttachmentLayout l) const {
	auto str = name.str<Interface>();
	_device->getTextureSetLayout()->readImage(*_device, *(Loop *)_loop.get(), (Image *)image.get(), l,
		[str] (const gl::ImageInfo &info, BytesView view) mutable {
			if (!StringView(str).ends_with(".png")) {
				str = str + String(".png");
			}
			if (!view.empty()) {
				auto fmt = gl::getImagePixelFormat(info.format);
				bitmap::PixelFormat pixelFormat = bitmap::PixelFormat::Auto;
				switch (fmt) {
				case gl::PixelFormat::A: pixelFormat = bitmap::PixelFormat::A8; break;
				case gl::PixelFormat::IA: pixelFormat = bitmap::PixelFormat::IA88; break;
				case gl::PixelFormat::RGB: pixelFormat = bitmap::PixelFormat::RGB888; break;
				case gl::PixelFormat::RGBA: pixelFormat = bitmap::PixelFormat::RGBA8888; break;
				default: break;
				}
				if (pixelFormat != bitmap::PixelFormat::Auto) {
					Bitmap bmp(view.data(), info.extent.width, info.extent.height, pixelFormat);
					bmp.save(str);
				}
			}
		});
}

void View::scheduleFence(Rc<Fence> &&fence) {
	if (_running.load()) {
		performOnThread([this, fence = move(fence)] () mutable {
			auto loop = (Loop *)_loop.get();
			if (!fence->check(*loop, true)) {
				auto frame = fence->getFrame();
				if (frame != 0 && (_fenceOrder == 0 || _fenceOrder > frame)) {
					_fenceOrder = frame;
				}
				_fences.emplace(move(fence));
			}
		}, this, true);
	} else {
		auto loop = (Loop *)_loop.get();
		fence->check(*loop, false);
	}
}

void View::mapWindow() {

}

bool View::pollInput(bool frameReady) {
	return false;
}

gl::SurfaceInfo View::getSurfaceOptions() const {
	return _instance->getSurfaceOptions(_surface->getSurface(), _device->getPhysicalDevice());
}

void View::invalidate() {

}

void View::scheduleNextImage(uint64_t windowOffset, bool immediately) {
	performOnThread([this, windowOffset, immediately] {
		_frameEmitter->setEnableBarrier(_options.enableFrameEmitterBarrier);

		if (_options.renderImageOffscreen) {
			scheduleSwapchainImage(windowOffset, AcquireOffscreenImage);
		} else if (_options.acquireImageImmediately || immediately) {
			scheduleSwapchainImage(windowOffset, AcquireSwapchainImageImmediate);
		} else {
			scheduleSwapchainImage(windowOffset, AcquireSwapchainImageAsync);
		}
	}, this, true);
}

void View::scheduleSwapchainImage(uint64_t windowOffset, ScheduleImageMode mode) {
	Rc<SwapchainImage> swapchainImage;
	Rc<FrameRequest> newFrameRequest;
	if (mode == ScheduleImageMode::AcquireOffscreenImage) {
		newFrameRequest = _frameEmitter->makeRequest(_screenExtent, _density);
	} else {
		auto fullOffset = getUpdateInterval() + windowOffset;
		if (fullOffset > _frameInterval) {
			swapchainImage = Rc<SwapchainImage>::create(Rc<SwapchainHandle>(_swapchain), _frameOrder, 0);
		} else {
			auto presentWindow = platform::device::_clock(platform::device::ClockType::Monotonic) + _frameInterval - getUpdateInterval() - windowOffset;
			swapchainImage = Rc<SwapchainImage>::create(Rc<SwapchainHandle>(_swapchain), _frameOrder, presentWindow);
		}

		swapchainImage->setReady(false);
		newFrameRequest = _frameEmitter->makeRequest(move(swapchainImage), _density);
	}

	// make new frame request immediately
	_loop->getApplication()->performOnMainThread([this, req = move(newFrameRequest)] () mutable {
		if (_director->acquireFrame(req)) {
			_loop->performOnGlThread([this, req = move(req)] () mutable {
				if (_loop->isRunning() && _swapchain) {
					req->bindSwapchain(this);
					auto order = _frameEmitter->submitNextFrame(move(req))->getOrder();

					performOnThread([this, order] {
						_frameOrder = order;
					}, this);
				}
			}, this);
		}
	}, this);

	// we should wait until all current fences become signaled
	// then acquire image and wait for fence
	if (swapchainImage) {
		if (mode == AcquireSwapchainImageAsync && _options.waitOnSwapchainPassFence && _fenceOrder != 0) {
			updateFences();
			if (_fenceOrder < swapchainImage->getOrder()) {
				if (!acquireScheduledImage(swapchainImage, mode)) {
					_scheduledImages.emplace_back(move(swapchainImage));
				}
			} else {
				_fenceImages.emplace_back(move(swapchainImage));
			}
		} else {
			if (!acquireScheduledImage(swapchainImage, mode)) {
				_scheduledImages.emplace_back(move(swapchainImage));
			}
		}
	}
}

bool View::acquireScheduledImage(const Rc<SwapchainImage> &image, ScheduleImageMode mode) {
	if (mode == AcquireOffscreenImage) {
		return false;
	}

	if (image->getSwapchain() != _swapchain) {
		image->invalidate();
		return true;
	}

	auto loop = (Loop *)_loop.get();
	auto fence = loop->acquireFence(0);
	if (_swapchain->acquire(image, fence, mode == AcquireSwapchainImageImmediate)) {
		fence->addRelease([tmp = image.get(), f = fence.get(), loop = _loop](bool success) {
			if (success) {
				loop->performOnGlThread([tmp] {
					tmp->setReady(true);
				}, tmp, true);
			}
#if XL_VKAPI_DEBUG
			XL_VKAPI_LOG("[", f->getFrame(),  "] vkAcquireNextImageKHR [complete]",
					" [", platform::device::_clock(platform::device::Monotonic) - f->getArmedTime(), "]");
#endif
		}, image, "View::acquireScheduledImage");
		if (mode == AcquireSwapchainImageImmediate) {
			fence->check(*((Loop *)_loop.get()), false);
			fence = nullptr;
		} else {
			scheduleFence(move(fence));
		}
		return true;
	} else {
		fence->schedule(*loop);
		return false;
	}
}

bool View::recreateSwapchain(gl::PresentMode mode) {
	struct ResetData : public Ref {
		Vector<Rc<SwapchainImage>> fenceImages;
		Vector<Rc<SwapchainImage>> scheduledImages;
		Rc<renderqueue::FrameEmitter> frameEmitter;
	};

	auto data = Rc<ResetData>::alloc();
	data->fenceImages = move(_fenceImages);
	data->scheduledImages = move(_scheduledImages);
	data->frameEmitter = _frameEmitter;

	_loop->performOnGlThread([data] {
		for (auto &it : data->fenceImages) {
			it->invalidate();
		}
		for (auto &it : data->scheduledImages) {
			it->invalidate();
		}
		// data->frameEmitter->dropFrames();
	}, this);

	_fenceImages.clear();
	_scheduledImages.clear();

	if (renderqueue::FrameHandle::GetActiveFramesCount() > 1) {
		renderqueue::FrameHandle::DescribeActiveFrames();
	}

	auto info = getSurfaceOptions();
	auto cfg = _selectConfig(info);

	if (!info.isSupported(cfg)) {
		log::vtext("Vk-Error", "Presentation with config ", cfg.description(), " is not supported for ", info.description());
		return false;
	}

	if (cfg.extent.width == 0 || cfg.extent.height == 0) {
		return false;
	}

	bool ret = false;
	if (mode == gl::PresentMode::Unsupported) {
		ret = createSwapchain(move(cfg), cfg.presentMode);
	} else {
		ret = createSwapchain(move(cfg), mode);
	}
	if (ret) {
		// run frame as fast as possible, no present window, no wait on fences
		scheduleNextImage(0, true);
	}
	return ret;
}

bool View::createSwapchain(gl::SwapchainConfig &&cfg, gl::PresentMode presentMode) {
	auto devInfo = _device->getInfo();

	auto swapchainImageInfo = getSwapchainImageInfo(cfg);
	uint32_t queueFamilyIndices[] = { devInfo.graphicsFamily.index, devInfo.presentFamily.index };

	do {
		auto oldSwapchain = move(_swapchain);

		_swapchain = Rc<SwapchainHandle>::create(*_device, cfg, move(swapchainImageInfo), presentMode,
				_surface, queueFamilyIndices, oldSwapchain ? oldSwapchain.get() : nullptr);

		if (_swapchain) {
			setScreenExtent(cfg.extent);

			Vector<uint64_t> ids;
			auto &cache = _loop->getFrameCache();
			for (auto &it : _swapchain->getImages()) {
				for (auto &iit : it.views) {
					auto id = iit.second->getIndex();
					ids.emplace_back(iit.second->getIndex());
					iit.second->setReleaseCallback([loop = _loop, cache, id] {
						loop->performOnGlThread([cache, id] {
							cache->removeImageView(id);
						});
					});
				}
			}

			_loop->performOnGlThread([loop = _loop, ids] {
				auto &cache = loop->getFrameCache();
				for (auto &id : ids) {
					cache->addImageView(id);
				}
			});
		}

		_config = move(cfg);

		++ _gen;
	} while (0);

	return _swapchain != nullptr;
}

bool View::isImagePresentable(const gl::ImageObject &image, VkFilter &filter) const {
	auto dev = (Device *)_device;

	auto &sourceImageInfo = image.getInfo();
	if (sourceImageInfo.extent.depth != 1 || sourceImageInfo.format != _config.imageFormat
			|| (sourceImageInfo.usage & gl::ImageUsage::TransferSrc) == gl::ImageUsage::None) {
		log::text("Swapchain", "Image can not be presented on swapchain");
		return false;
	}

	VkFormatProperties sourceProps;
	VkFormatProperties targetProps;

	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(sourceImageInfo.format), &sourceProps);
	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(_config.imageFormat), &targetProps);

	if (_config.extent.width == sourceImageInfo.extent.width && _config.extent.height == sourceImageInfo.extent.height) {
		if ((targetProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0) {
			return false;
		}

		if (sourceImageInfo.tiling == gl::ImageTiling::Optimal) {
			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
				return false;
			}
		} else {
			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0) {
				return false;
			}
		}
	} else {
		if ((targetProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) == 0) {
			return false;
		}

		if (sourceImageInfo.tiling == gl::ImageTiling::Optimal) {
			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
				return false;
			}

			if ((sourceProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0) {
				filter = VK_FILTER_LINEAR;
			}
		} else {
			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
				return false;
			}

			if ((sourceProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0) {
				filter = VK_FILTER_LINEAR;
			}
		}
	}

	return true;
}

void View::runFrameWithSwapchainImage(Rc<ImageStorage> &&image) {
	image->setReady(false);

	_frameEmitter->setEnableBarrier(_options.enableFrameEmitterBarrier);
	auto req = _frameEmitter->makeRequest(move(image), _density);
	_loop->getApplication()->performOnMainThread([this, req = move(req)] () mutable {
		if (_director->acquireFrame(req)) {
			_loop->performOnGlThread([this, req = move(req)] () mutable {
				if (_loop->isRunning() && _swapchain) {
					req->bindSwapchain(this);
					auto order = _frameEmitter->submitNextFrame(move(req))->getOrder();

					performOnThread([this, order] {
						_frameOrder = order;
					}, this);
				}
			}, this);
		}
	}, this);
}

void View::runScheduledPresent(Rc<SwapchainImage> &&object) {
	_loop->performOnGlThread([this, object = move(object)] () mutable {
		if (!_loop->isRunning()) {
			return;
		}

		_device->acquireQueue(QueueOperations::Present, *(Loop *)_loop.get(),
				[this, object = move(object)] (Loop &, const Rc<DeviceQueue> &queue) mutable {
			performOnThread([this, queue, object = move(object)] () mutable {
				if (object->getSwapchain() == _swapchain && object->isSubmitted()) {
					presentWithQueue(*queue, move(object));
				}
				_loop->performOnGlThread([this,  queue = move(queue)] () mutable {
					_device->releaseQueue(move(queue));
				}, this);
			}, this);
		}, [this] (Loop &) {
			invalidate();
		}, this);
	}, this);
}

void View::presentWithQueue(DeviceQueue &queue, Rc<ImageStorage> &&image) {
	auto res = _swapchain->present(queue, move(image));
	auto dt = updateFrameInterval();
	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
		_swapchain->deprecate(false);
	}

	_blockDeprecation = true;

	if (!pollInput(true)) {
		return;
	}

	_blockDeprecation = false;

	if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
		waitForFences(_frameOrder);
		queue.waitIdle();

		// log::vtext("View", "recreateSwapchain - View::presentWithQueue (", renderqueue::FrameHandle::GetActiveFramesCount(), ")");
		recreateSwapchain(_swapchain->getRebuildMode());
	} else {
		if (_options.followDisplayLink) {
			scheduleNextImage(0, true);
			return;
		}
		if (_options.flattenFrameRate) {
			const auto maxWindow = _frameInterval - getUpdateInterval() + _frameInterval / 20;
			const auto currentWindow = std::max(dt.first, dt.second);

			if (currentWindow > maxWindow) {
				auto ft = _frameEmitter->getAvgFrameTime();
				if (ft < maxWindow) {
					scheduleNextImage(currentWindow, false);
				} else {
					scheduleNextImage(currentWindow + ft - maxWindow, false);
				}
				return;
			}
		}
		scheduleNextImage(0, false);
	}
}

void View::invalidateSwapchainImage(Rc<ImageStorage> &&image) {
	_swapchain->invalidateImage(move(image));

	if (_swapchain->isDeprecated() && _swapchain->getAcquiredImagesCount() == 0) {
		// log::vtext("View", "recreateSwapchain - View::invalidateSwapchainImage (", renderqueue::FrameHandle::GetActiveFramesCount(), ")");
		recreateSwapchain(_swapchain->getRebuildMode());
	} else {
		scheduleNextImage(_frameInterval, false);
	}
}

Pair<uint64_t, uint64_t> View::updateFrameInterval() {
	auto n = platform::device::_clock();
	auto dt = n - _lastFrameStart;
	_lastFrameInterval = dt;
	_avgFrameInterval.addValue(dt);
	_avgFrameIntervalValue = _avgFrameInterval.getAverage(true);
	_lastFrameStart = n;
	return pair(_avgFrameIntervalValue.load(), dt);
}

void View::waitForFences(uint64_t min) {
	auto loop = (Loop *)_loop.get();
	auto it = _fences.begin();
	while (it != _fences.end()) {
		if ((*it)->getFrame() <= min) {
			// log::vtext("View", "waitForFences: ", (*it)->getTag());
			if ((*it)->check(*loop, false)) {
				it = _fences.erase(it);
			} else {
				++ it;
			}
		} else {
			++ it;
		}
	}
}

void View::finalize() {
	_loop->performOnGlThread([this] {
		end();
	}, this);
}

void View::updateFences() {
	uint64_t fenceOrder = 0;
	do {
		auto loop = (Loop *)_loop.get();
		auto it = _fences.begin();
		while (it != _fences.end()) {
			if ((*it)->check(*loop, true)) {
				it = _fences.erase(it);
			} else {
				auto frame = (*it)->getFrame();
				if (frame != 0 && (fenceOrder == 0 || fenceOrder > frame)) {
					fenceOrder = frame;
				}
				++ it;
			}
		}
	} while (0);

	_fenceOrder = fenceOrder;
}

}
