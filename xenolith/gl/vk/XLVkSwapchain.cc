/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLVkSwapchain.h"
#include "XLVkInstance.h"
#include "XLVkDevice.h"
#include "XLVkFramebuffer.h"
#include "XLGlRenderQueue.h"

namespace stappler::xenolith::vk {

void Swapchain_writeImageTransfer(Device &dev, VkCommandBuffer buf, const Rc<Image> &sourceImage, const Rc<Image> &targetImage,
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

struct SwapchainPresentFrame : public Ref {
	Rc<gl::Loop> loop;
	Rc<Device> device;
	Rc<Swapchain> swapchain;
	Rc<SwapchainSync> sync; // need to be released in invalidate
	Rc<DeviceQueue> queue; // need to be released in invalidate
	Rc<Fence> fence;  // always released automatically
	Rc<CommandPool> pool;  // need to be released in invalidate
	Vector<VkCommandBuffer> buffers;
	QueueOperations ops = QueueOperations::Present;
	uint64_t order = 0;

	VkImageLayout sourceLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkFilter filter = VK_FILTER_NEAREST;
	VkResult result = VK_SUCCESS;

	Rc<Swapchain::PresentTask> sourceObject;
	Rc<Image> sourceImage;
	Rc<gl::ImageAttachmentObject> targetImage;
	gl::FrameSync frameSync;

	bool imageAcquired = false;
	bool commandsReady = false;
	bool valid = true;
	bool releaseQueue = true;

	// depends on output mode
	gl::PipelineStage waitStages = gl::PipelineStage::Transfer;

	Function<void(VkResult)> onComplete;

	SwapchainPresentFrame(gl::Loop &l, const Rc<Swapchain> &s, uint64_t order)
	: loop(&l), device((Device *)l.getDevice().get()), swapchain(s), order(order) { }

	SwapchainPresentFrame(gl::Loop &l, const Rc<Swapchain> &s, uint64_t order,
			Function<void(VkResult)> &&onComplete, const Rc<DeviceQueue> &queue)
	: loop(&l), device((Device *)l.getDevice().get()), swapchain(s), queue(queue), order(order),
	  onComplete(move(onComplete)) { }

	virtual ~SwapchainPresentFrame() {
		invalidate();
	}

	void invalidate() {
		if (valid) {
			if (result == VK_SUCCESS) {
				result = VK_ERROR_UNKNOWN;
			}
			if (onComplete) {
				onComplete(result);
				onComplete = nullptr;
			}
			if (sync) {
				sync = nullptr;
			}
			if (pool) {
				device->releaseCommandPool(*loop, move(pool));
				pool = nullptr;
			}
			if (queue) {
				device->releaseQueue(move(queue));
				queue = nullptr;
			}
			valid = false;
		}
	}

	void run(const Rc<Swapchain::PresentTask> &source, VkImageLayout l = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VkFilter f = VK_FILTER_NEAREST) {
		sourceImage = (Image *)source->object->image.get();
		sourceLayout = l;
		filter = f;

		sourceObject = source;

		if (!queue) {
			acquireQueue();
		} else {
			releaseQueue = false;
		}
		acquireImage();
	}

	bool isValid() {
		return valid;
	}

	void acquireQueue() {
		device->acquireQueue(ops, *loop, [this] (gl::Loop &, const Rc<DeviceQueue> &queue) {
			onQueueAcquired(queue);
		}, [this] (gl::Loop &) {
			invalidate();
		}, this);
	}

	void acquireImage() {
		sync = swapchain->acquireSwapchainSync(*device);
		if (!sync) {
			invalidate();
			return;
		}
		VkResult result = sync->acquireImage(*device);
		switch (result) {
		case VK_ERROR_OUT_OF_DATE_KHR:
			this->result = result;
			invalidate();
			break;
		case VK_SUCCESS: // acquired successfully
		case VK_SUBOPTIMAL_KHR: // swapchain recreation signal will be sent by view, but we can try to do some frames until that
			onImageAcquired();
			break;

		case VK_NOT_READY:

			// VK_TIMEOUT result is not documented, but some drivers returns that
			// see https://community.amd.com/t5/opengl-vulkan/vkacquirenextimagekhr-with-0-timeout-returns-vk-timeout-instead/td-p/350023
		case VK_TIMEOUT:
			loop->schedule([this, linkId = retain()] (gl::Loop::Context &ctx) {
				if (!isValid()) {
					release(linkId);
					return true;
				}

				VkResult result = sync->acquireImage(*device);
				switch (result) {
				case VK_ERROR_OUT_OF_DATE_KHR:
					this->result = result;
					// push swapchain invalidation
					invalidate();
					release(linkId);
					return true; // end spinning
					break;
				case VK_SUCCESS:
				case VK_SUBOPTIMAL_KHR:
					// acquired successfully
					onImageAcquired();
					release(linkId);
					return true; // end spinning
					break;
				case VK_NOT_READY:
				case VK_TIMEOUT:
					return false; // continue spinning
					break;
				default:
					this->result = result;
					invalidate();
					release(linkId);
					return true; // end spinning
					break;
				}

				release(linkId);
				return true;
			}, "Swapchain::acquireImage");
			break;
		default:
			invalidate();
			break;
		}
	}

	void prepareCommands() {
		if (!valid) {
			return;
		}

		pool = device->acquireCommandPool(ops);
		loop->getQueue()->perform(Rc<Task>::create([this] (const Task &) {
			if (targetImage) {
				auto buf = pool->allocBuffer(*device);
				Swapchain_writeImageTransfer(*device, buf, sourceImage, targetImage->image.cast<Image>(), sourceLayout, filter);
				buffers.emplace_back(buf);
				return true;
			}
			return false;
		}, [this] (const Task &, bool success) {
			if (success) {
				onCommandsReady();
			} else {
				invalidate();
			}
		}, this));
	}

	void onQueueAcquired(const Rc<DeviceQueue> &q) {
		queue = q;
		if (queue && imageAcquired && commandsReady && valid) {
			perform();
		}
	}

	void onImageAcquired() {
		imageAcquired = true;
		if (!targetImage) {
			targetImage = sync->getImage();
			prepareCommands();
		}
		if (!targetImage) {
			invalidate();
		} else if (queue && imageAcquired && commandsReady && valid) {
			perform();
		}
	}

	void onCommandsReady() {
		commandsReady = true;
		if (queue && imageAcquired && commandsReady && valid) {
			perform();
		}
	}

	void perform() {
		if (!valid) {
			return;
		}

		fence = device->acquireFence(order);
		fence->addRelease([device = device, pool = move(pool), loop = loop] (bool success) {
			device->releaseCommandPool(*loop, Rc<CommandPool>(pool));
		}, this, "Swapchain::perform releaseCommandPool");
		pool = nullptr;

		loop->getQueue()->perform(Rc<Task>::create([this] (const Task &) {

			sourceObject->object->rearmSemaphores(*device);

			frameSync.waitAttachments.emplace_back(gl::FrameSyncAttachment{nullptr, sourceObject->object->waitSem, gl::PipelineStage::Transfer});
			frameSync.signalAttachments.emplace_back(gl::FrameSyncAttachment{nullptr, sourceObject->object->signalSem});

			result = sync->submitWithPresent(*device, *queue, frameSync, *fence, buffers, waitStages);
			if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
				return true;
			}
			return false;
		}, [this] (const Task &, bool success) {
			if (queue) {
				if (releaseQueue) {
					device->releaseQueue(move(queue));
				}
				queue = nullptr;
			}
			if (success) {
				device->scheduleFence(*loop, move(fence));
				fence = nullptr;
				invalidate();
			} else {
				device->scheduleFence(*loop, move(fence));
				fence = nullptr;
				invalidate();
			}
			if (onComplete) {
				onComplete(result);
				onComplete = nullptr;
			}
		}, this));
	}
};

Swapchain::~Swapchain() {
	_syncs.clear();
}

bool Swapchain::init(gl::View *view, Device &device, VkSurfaceKHR surface) {
	auto info = device.getInstance()->getSurfaceOptions(surface, device.getPhysicalDevice());
	auto cfg = view->selectConfig(info);

	if (!info.isSupported(cfg)) {
		log::vtext("Vk-Error", "Presentation with config ", cfg.description(), " is not supported for ", info.description());
		return false;
	}

	if constexpr (s_printVkInfo) {
		Application::getInstance()->perform([info] (const Task &) {
			log::vtext("Vk-Info", "Surface info:", info.description());
			return true;
		}, nullptr, this);
	}

	if (gl::Swapchain::init(view)) {
		auto presentMode = cfg.presentMode;
		_surface = surface;
		_device = &device;
		if (createSwapchain(device, move(cfg), presentMode)) {
			return true;
		}
	}
	return false;
}

bool Swapchain::recreateSwapchain(gl::Device &idevice, gl::SwapchanCreationMode mode) {
	auto &device = dynamic_cast<Device &>(idevice);

	XL_VK_LOG("RecreateSwapChain: ", (mode == gl::SwapchanCreationMode::Best) ? "Best" : "Fast");

	auto info = device.getInstance()->getSurfaceOptions(_surface, device.getPhysicalDevice());
	auto cfg = _view->selectConfig(info);

	if (!info.isSupported(cfg)) {
		log::vtext("Vk-Error", "Presentation with config ", cfg.description(), " is not supported for ", info.description());
		return false;
	}

	if (cfg.extent.width == 0 || cfg.extent.height == 0) {
		return false;
	}

	if (mode == gl::SwapchanCreationMode::Fast && cfg.presentModeFast != gl::PresentMode::Unsupported) {
		return createSwapchain(device, move(cfg), cfg.presentModeFast);
	} else {
		return createSwapchain(device, move(cfg), cfg.presentMode);
	}
}

bool Swapchain::createSwapchain(Device &device, gl::SwapchainConfig &&cfg, gl::PresentMode presentMode) {
	auto &devInfo = device.getInfo();

	auto swapchainImageInfo = getSwapchainImageInfo(cfg);
	uint32_t queueFamilyIndices[] = { devInfo.graphicsFamily.index, devInfo.presentFamily.index };

	do {
		std::unique_lock<Mutex> lock(_swapchainMutex);

		auto oldSwapchain = move(_swapchain);

		_swapchain = Rc<SwapchainHandle>::create(device, cfg, move(swapchainImageInfo), presentMode,
				_surface, queueFamilyIndices, [this] (gl::SwapchanCreationMode mode) {
			switch (mode) {
			case gl::SwapchanCreationMode::Best:
				_view->pushEvent(AppEvent::SwapchainRecreationBest);
				break;
			case gl::SwapchanCreationMode::Fast:
				_view->pushEvent(AppEvent::SwapchainRecreation);
				break;
			}
		}, oldSwapchain.get());

		_config = move(cfg);

		++ _gen;

		Rc<PresentTask> task;
		_presentCurrentMutex.lock();
		if (_presentCurrent && _presentCurrent->order) {
			task = _presentCurrent;
		}
		_presentCurrentMutex.unlock();

		if (task) {
			presentImmediate(task);
		}
	} while (0);

	return true;
}

void Swapchain::invalidate(gl::Device &idevice) {
	auto &device = dynamic_cast<Device &>(idevice);

	if (_swapchain) {
		_swapchain->deprecate(true);
		_swapchain = nullptr;
	}

	if (_surface) {
		auto i = device.getInstance();
		device.getTable()->vkDeviceWaitIdle(device.getDevice());
		i->vkDestroySurfaceKHR(i->getInstance(), _surface, nullptr);
		_surface = VK_NULL_HANDLE;
	}
}

bool Swapchain::present(gl::Loop &loop, const Rc<PresentTask> &presentTask) {
	std::unique_lock<Mutex> lock(_swapchainMutex);
	if (!_config.transfer || !_swapchain) {
		return false;
	}

	lock.unlock();

	if (_presentScheduled) {
		_presentPending = presentTask;
		return true;
	}

	VkFilter filter = VK_FILTER_NEAREST;
	if (!isImagePresentable(*presentTask->object->image, filter)) {
		return false;
	}

	if (presentTask->order < _order) {
		return false;
	}

	_presentCurrentMutex.lock();
	if (presentTask != _presentCurrent) {
		_presentCurrent = presentTask;
	}

	++ _order;
	presentTask->order = _order;

	_presentCurrentMutex.unlock();

	_presentScheduled = true;
	auto frame = Rc<SwapchainPresentFrame>::alloc(loop, this, _order,
			[this, loop = &loop, presentTask] (VkResult res) mutable {
		onPresentComplete(*loop, res, presentTask);
	}, _presentQueue);

	frame->run(presentTask, VkImageLayout(presentTask->object->layout), filter);
	return true;
}

void Swapchain::deprecate() {
	std::unique_lock<Mutex> lock(_swapchainMutex);
	_swapchain->deprecate();
}

Rc<SwapchainSync> Swapchain::acquireSwapchainSync(Device &dev, bool lock) {
	std::unique_lock<Mutex> swapchainLock;
	if (lock) {
		swapchainLock = std::unique_lock<Mutex>(_swapchainMutex);
	}

	auto swapchain = _swapchain;
	if (!swapchain->retainUsage()) {
		return nullptr;
	}

	if (!_syncs.empty()) {
		auto ret = _syncs.back();
		_syncs.pop_back();
		ret->reset(dev, swapchain, _gen);
		return ret;
	}
	return Rc<SwapchainSync>::create(dev, swapchain, &_swapchainMutex, _gen, [this] (Rc<SwapchainSync> &&sync) {
		releaseSync(move(sync));
	}, [this] (Rc<SwapchainSync> &&sync) {
		presentSync(move(sync));
	});
}

void Swapchain::releaseSync(Rc<SwapchainSync> &&ref) {
	if (_view->getLoop()->isOnThread()) {
		ref->disable();
		_syncs.emplace_back(move(ref));
	} else {
		_view->getLoop()->performOnThread([this, ref = move(ref)] () mutable {
			ref->disable();
			_syncs.emplace_back(move(ref));
		}, this);
	}
}

void Swapchain::presentSync(Rc<SwapchainSync> &&sync) {
	if (_view->getLoop()->isOnThread()) {
		std::unique_lock<Mutex> lock(_presentCurrentMutex);
		if (_presentCurrent && _presentCurrent->order < _order) {
			_presentCurrent = nullptr;
		}
		if (sync == _presentedSync) {
			return;
		}

		Rc<SwapchainSync> tmp;
		if (_presentedSync) {
			tmp = move(_presentedSync);
		}
		_presentedSync = move(sync);
		lock.unlock();
		if (tmp) {
			tmp->setPresentationEnded(true);
		}
	} else {
		_view->getLoop()->performOnThread([this, sync = move(sync)] () mutable {
			presentSync(move(sync));
		}, this);
	}
}

Rc<gl::ImageAttachmentObject> Swapchain::acquireImage(const gl::Loop &loop, const gl::ImageAttachment *a, Extent3 e) {
	std::unique_lock<Mutex> lock(_swapchainMutex);
	if (!_swapchain->isOptimal()) {
		return nullptr;
	}

	lock.unlock();

	auto device = (Device *)loop.getDevice().get();
	auto sync = acquireSwapchainSync(*device);
	if (sync) {
		if (sync->getImageExtent() == e) {
			sync->acquireImage(*device);
			if (auto img = sync->getImage()) {
				img->makeViews(*device, *a);
				_presentCurrentMutex.lock();
				++ _order;
				_presentCurrentMutex.unlock();
				return img;
			}
		}
		releaseSync(move(sync));
	}
	return nullptr;
}

void Swapchain::onPresentComplete(gl::Loop &loop, VkResult res, const Rc<PresentTask> &task) {
	_presentScheduled = false;
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		_presentPending = nullptr;
	} else if (_presentPending) {
		auto tmp = move(_presentPending);
		_presentPending = nullptr;
		present(loop, tmp);
	}
}

bool Swapchain::isImagePresentable(const gl::ImageObject &image, VkFilter &filter) const {
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

bool Swapchain::presentImmediate(const Rc<PresentTask> &task) {
	auto ops = QueueOperations::Present;
	auto dev = (Device *)_device;

	VkFilter filter;
	if (!isImagePresentable(*task->object->image, filter)) {
		return false;
	}

	Rc<DeviceQueue> queue;
	Rc<SwapchainSync> sync;
	Rc<CommandPool> pool;
	Rc<Fence> fence;

	Rc<Image> sourceImage = (Image *)task->object->image.get();
	Rc<gl::ImageAttachmentObject> targetImage;

	Vector<VkCommandBuffer> buffers;
	gl::PipelineStage waitStages = gl::PipelineStage::Transfer;

	auto cleanup = [&] {
		if (fence) {
			dev->releaseFenceUnsafe(move(fence));
			fence = nullptr;
		}
		if (pool) {
			dev->releaseCommandPoolUnsafe(move(pool));
			pool = nullptr;
		}
		if (sync) {
			sync = nullptr;
		}
		if (queue) {
			dev->releaseQueue(move(queue));
			queue = nullptr;
		}
		return false;
	};

	queue = dev->tryAcquireQueueSync(ops);
	if (!queue) {
		return cleanup();
	}

	sync = acquireSwapchainSync(*dev, false);
	VkResult result = sync->acquireImage(*dev);
	if (result != VK_SUCCESS) {
		return cleanup();
	}

	targetImage = sync->getImage();

	pool = dev->acquireCommandPool(ops);
	fence = dev->acquireFence(0);

	auto buf = pool->allocBuffer(*dev);

	Swapchain_writeImageTransfer(*dev, buf, sourceImage, targetImage->image.cast<Image>(), VkImageLayout(task->object->layout), filter);

	buffers.emplace_back(buf);

	gl::FrameSync frameSync;
	task->object->rearmSemaphores(*dev);

	frameSync.waitAttachments.emplace_back(gl::FrameSyncAttachment{nullptr, task->object->waitSem, gl::PipelineStage::Transfer});
	frameSync.signalAttachments.emplace_back(gl::FrameSyncAttachment{nullptr, task->object->signalSem});

	result = sync->submitWithPresent(*dev, *queue, frameSync, *fence, buffers, waitStages);
	if (queue) {
		dev->releaseQueue(move(queue));
		queue = nullptr;
	}
	if (result == VK_SUCCESS) {
		fence->check(false);
		dev->releaseFenceUnsafe(move(fence));
		dev->releaseCommandPoolUnsafe(move(pool));
		return true;
	} else {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			fence->check(false);

			dev->releaseFenceUnsafe(move(fence));
			fence = nullptr;

			dev->releaseCommandPoolUnsafe(move(pool));
			pool = nullptr;
		}
		return cleanup();
	}
}

}
