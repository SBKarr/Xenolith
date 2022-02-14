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

struct SwapchainPresentFrame : public Ref {
	Rc<gl::Loop> loop;
	Rc<Device> device;
	Rc<Swapchain> swapchain;
	Rc<SwapchainSync> sync; // need to be released in invalidate
	Rc<DeviceQueue> queue; // need to be released in invalidate
	Rc<Fence> fence;  // always released automatically
	Rc<CommandPool> pool;  // need to be released in invalidate
	Vector<VkCommandBuffer> buffers;
	QueueOperations ops = QueueOperations::Graphics;
	uint64_t order = 0;

	VkImageLayout sourceLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkFilter filter = VK_FILTER_NEAREST;

	Rc<Image> sourceImage;
	Rc<Image> targetImage;

	bool imageAcquired = false;
	bool commandsReady = false;
	bool valid = true;

	// depends on output mode
	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	SwapchainPresentFrame(gl::Loop &l, const Rc<Swapchain> &s, uint64_t order)
	: loop(&l), device((Device *)l.getDevice().get()), swapchain(s), order(order) { }

	virtual ~SwapchainPresentFrame() {
		invalidate();
	}

	void invalidate() {
		if (valid) {
			if (sync) {
				swapchain->releaseSwapchainSync(move(sync));
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

	void run(Rc<Image> &&source, VkImageLayout l = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VkFilter f = VK_FILTER_NEAREST) {
		sourceImage = move(source);
		sourceLayout = l;
		filter = f;

		acquireQueue();
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
		sync = swapchain->acquireSwapchainSync(*device, order);
		VkResult result = sync->acquireImage(*device, *swapchain);
		switch (result) {
		case VK_ERROR_OUT_OF_DATE_KHR:
			loop->recreateSwapChain(swapchain);
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

				VkResult result = sync->acquireImage(*device, *swapchain);
				switch (result) {
				case VK_ERROR_OUT_OF_DATE_KHR:
					// push swapchain invalidation
					XL_VK_LOG("vkAcquireNextImageKHR: VK_ERROR_OUT_OF_DATE_KHR");
					ctx.events->emplace_back(gl::Loop::EventName::SwapChainDeprecated, swapchain, data::Value());
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
		pool = device->acquireCommandPool(ops);
		loop->getQueue()->perform(Rc<Task>::create([this] (const Task &) {
			auto buf = pool->allocBuffer(*device);

			writeImageTransfer(buf);

			buffers.emplace_back(buf);
			return true;
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
		if (!imageAcquired) {
			prepareCommands();
		}
		imageAcquired = true;
		targetImage = swapchain->getImage(sync->getImageIndex());
		if (!targetImage) {

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
		fence = device->acquireFence(order);
		fence->addRelease([device = device, pool = move(pool), loop = loop] (bool success) {
			device->releaseCommandPool(*loop, Rc<CommandPool>(pool));
		}, this, "Swapchain::perform releaseCommandPool");
		pool = nullptr;

		loop->getQueue()->perform(Rc<Task>::create([this] (const Task &) {
			bool swapchainValid = true;
			sync->lock();
			if (sync->isSwapchainValid()) {
				swapchainValid = false;
			}

			if (!swapchainValid || !submit()) {
				sync->unlock();
				return false;
			}

			present();
			sync->unlock();
			return true;
		}, [this] (const Task &, bool success) {
			if (queue) {
				device->releaseQueue(move(queue));
				queue = nullptr;
			}
			if (success) {
				fence->addRelease([swapchain = swapchain, sync = move(sync)] (bool success) {
					swapchain->releaseSwapchainSync(Rc<SwapchainSync>(sync));
				}, nullptr, "Swapchain::perform releaseSwapchainSync");
				sync = nullptr;

				device->scheduleFence(*loop, move(fence));
				fence = nullptr;
				invalidate();
			} else {
				device->releaseFence(move(fence));
				fence = nullptr;
				invalidate();
			}
		}, this));
	}

	void writeImageTransfer(VkCommandBuffer buf) {
		auto table = device->getTable();

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
	}

	bool submit() {
		auto table = device->getTable();

		auto imageReadySem = sync->getImageReady()->getSemaphore();
		auto presentSem = sync->getRenderFinished()->getSemaphore();

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &imageReadySem;
		submitInfo.pWaitDstStageMask = &waitStages;
		submitInfo.commandBufferCount = buffers.size();
		submitInfo.pCommandBuffers = buffers.data();
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &presentSem;

		if (table->vkQueueSubmit(queue->getQueue(), 1, &submitInfo, fence->getFence()) == VK_SUCCESS) {
			// mark semaphores
			sync->getImageReady()->setSignaled(false);
			sync->getRenderFinished()->setSignaled(true);
			return true;
		}
		return false;
	}

	bool present() {
		auto table = device->getTable();

		auto swapchainObj = swapchain->getSwapchain();
		auto presentSem = sync->getRenderFinished()->getSemaphore();
		auto imageIndex = sync->getImageIndex();

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &presentSem;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchainObj;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Optional

		// auto t = platform::device::_clock();
		auto result = table->vkQueuePresentKHR(queue->getQueue(), &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			loop->performOnThread([this] () {
				loop->pushContextEvent(gl::Loop::EventName::SwapChainDeprecated, swapchain);
				invalidate();
			}, this);
		} else if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
			sync->getRenderFinished()->setSignaled(false);
			sync->clearImageIndex();
			return true;
		} else {
			log::vtext("VK-Error", "Fail to vkQueuePresentKHR: ", result);
			loop->performOnThread([this] () {
				invalidate();
			});
		}
		return false;
	}
};

static VkPresentModeKHR getVkPresentMode(gl::PresentMode presentMode) {
	switch (presentMode) {
	case gl::PresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR; break;
	case gl::PresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR; break;
	case gl::PresentMode::Fifo: return VK_PRESENT_MODE_FIFO_KHR; break;
	case gl::PresentMode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR; break;
	default: break;
	}
	return VkPresentModeKHR(0);
}

SwapchainSync::~SwapchainSync() { }

bool SwapchainSync::init(Device &dev, uint32_t idx, uint64_t gen) {
	_frameIndex = idx;
	_imageReady = Rc<Semaphore>::create(dev);
	_renderFinished = Rc<Semaphore>::create(dev);
	_gen = gen;
	return true;
}

void SwapchainSync::reset(uint64_t gen) {
	_swapchainValid = true;
	if (gen != _gen) {
		_imageIndex = maxOf<uint32_t>();
		_imageReady->reset();
	} else {
		if (_imageIndex == maxOf<uint32_t>()) {
			_imageReady->reset();
		}
	}
	_renderFinished->reset();
}

void SwapchainSync::invalidate() {
	_imageReady->invalidate();
	_imageReady = nullptr;
	_renderFinished->invalidate();
	_renderFinished = nullptr;
}

void SwapchainSync::lock() {
	_mutex.lock();
}

void SwapchainSync::unlock() {
	_mutex.unlock();
}

VkResult SwapchainSync::acquireImage(Device &dev, Swapchain &swapchain) {
	VkResult result = VK_ERROR_UNKNOWN;
	_mutex.lock();
	if (_imageIndex != maxOf<uint32_t>()) {
		_mutex.unlock();
		return VK_SUCCESS;
	}
	if (_swapchainValid) {
		result = dev.getTable()->vkAcquireNextImageKHR(dev.getDevice(), swapchain.getSwapchain(),
				0, _imageReady->getUnsignalled(), VK_NULL_HANDLE, &_imageIndex);
	}
	_mutex.unlock();
	switch (result) {
	case VK_SUCCESS:
	case VK_SUBOPTIMAL_KHR:
		_imageReady->setSignaled(true);
		break;
	default:
		break;
	}
	return result;
}

void SwapchainSync::clearImageIndex() {
	_imageIndex = maxOf<uint32_t>();
}

Swapchain::~Swapchain() {
	for (auto &it : _sems) {
		for (auto &iit : it) {
			iit->invalidate();
		}
	}
	_sems.clear();
}

bool Swapchain::init(const gl::View *view, Device &device, Function<gl::SwapchainConfig(const gl::SurfaceInfo &)> &&cb, VkSurfaceKHR surface) {
	auto info = device.getInstance()->getSurfaceOptions(surface, device.getPhysicalDevice());
	auto cfg = cb(info);

	if (!info.isSupported(cfg)) {
		log::vtext("Vk-Error", "Presentation with config ", cfg.description(), " is not supported for ", info.description());
		return false;
	}

	if constexpr (s_printVkInfo) {
		Application::getInstance()->perform([instance = device.getInstance(), surface, dev = device.getPhysicalDevice()] (const Task &) {
			log::vtext("Vk-Info", "Surface info:", instance->getSurfaceOptions(surface, dev).description());
			return true;
		}, nullptr, this);
	}

	if (gl::Swapchain::init(view, move(cb))) {
		auto presentMode = cfg.presentMode;
		if (createSwapchain(device, move(cfg), presentMode)) {
			_surface = surface;
			_sems.resize(FramesInFlight);
			return true;
		}
	}
	return false;
}

bool Swapchain::recreateSwapchain(gl::Device &idevice, gl::SwapchanCreationMode mode) {
	auto &device = dynamic_cast<Device &>(idevice);

	XL_VK_LOG("RecreateSwapChain: ", (mode == gl::SwapchanCreationMode::Best) ? "Best" : "Fast");

	auto info = device.getInstance()->getSurfaceOptions(_surface, device.getPhysicalDevice());
	auto cfg = _configCallback(info);

	if (!info.isSupported(cfg)) {
		log::vtext("Vk-Error", "Presentation with config ", cfg.description(), " is not supported for ", info.description());
		return false;
	}

	if (cfg.extent.width == 0 || cfg.extent.height == 0) {
		return false;
	}

	if (_swapchain) {
		cleanupSwapchain(device);
	} else {
		device.getTable()->vkDeviceWaitIdle(device.getDevice());
	}

	if (mode == gl::SwapchanCreationMode::Best && cfg.presentModeFast != gl::PresentMode::Unsupported) {
		return createSwapchain(device, move(cfg), cfg.presentModeFast);
	} else {
		return createSwapchain(device, move(cfg), cfg.presentMode);
	}
}

bool Swapchain::createSwapchain(Device &device, gl::SwapchainConfig &&cfg, gl::PresentMode presentMode) {
	auto table = device.getTable();

	auto swapchainImageInfo = getSwapchainImageInfo(cfg);

	VkSwapchainCreateInfoKHR swapChainCreateInfo = { };
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = _surface;
	swapChainCreateInfo.minImageCount = cfg.imageCount;
	swapChainCreateInfo.imageFormat = VkFormat(swapchainImageInfo.format);
	swapChainCreateInfo.imageColorSpace = VkColorSpaceKHR(cfg.colorSpace);
	swapChainCreateInfo.imageExtent = VkExtent2D({swapchainImageInfo.extent.width, swapchainImageInfo.extent.height});
	swapChainCreateInfo.imageArrayLayers = swapchainImageInfo.arrayLayers.get();
	swapChainCreateInfo.imageUsage = VkImageUsageFlags(swapchainImageInfo.usage);

	auto &devInfo = device.getInfo();

	uint32_t queueFamilyIndices[] = { devInfo.graphicsFamily.index, devInfo.presentFamily.index };

	if (devInfo.graphicsFamily.index != devInfo.presentFamily.index) {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = 2;
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	swapChainCreateInfo.preTransform = VkSurfaceTransformFlagBitsKHR(cfg.transform);
	swapChainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(cfg.alpha);
	swapChainCreateInfo.presentMode = getVkPresentMode(presentMode);
	swapChainCreateInfo.clipped = (cfg.clipped ? VK_TRUE : VK_FALSE);

	if (_oldSwapchain) {
		swapChainCreateInfo.oldSwapchain = _oldSwapchain;
	} else {
		swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	}

	swapChainCreateInfo.imageExtent = device.getInstance()->getSurfaceExtent(_surface, device.getPhysicalDevice());

	if (table->vkCreateSwapchainKHR(device.getDevice(), &swapChainCreateInfo, nullptr, &_swapchain) != VK_SUCCESS) {
		return false;
	}

	swapchainImageInfo.extent = Extent3(swapChainCreateInfo.imageExtent.width, swapChainCreateInfo.imageExtent.height, 1);

	if (_oldSwapchain) {
		table->vkDestroySwapchainKHR(device.getDevice(), _oldSwapchain, nullptr);
		_oldSwapchain = VK_NULL_HANDLE;
	}

	Vector<VkImage> swapchainImages;

	uint32_t imageCount = 0;
	table->vkGetSwapchainImagesKHR(device.getDevice(), _swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	table->vkGetSwapchainImagesKHR(device.getDevice(), _swapchain, &imageCount, swapchainImages.data());

	_config = move(cfg);
	_presentMode = presentMode;
	_valid = true;
	return true;
}

void Swapchain::cleanupSwapchain(Device &device) {
	_images.clear();

	device.getTable()->vkDeviceWaitIdle(device.getDevice());

	if (_swapchain) {
		_oldSwapchain = _swapchain;
		_swapchain = VK_NULL_HANDLE;
	}
}

void Swapchain::invalidate(gl::Device &idevice) {
	auto &device = dynamic_cast<Device &>(idevice);

	incrementGeneration(0); // wait idle

	if (_swapchain) {
		cleanupSwapchain(device);
	}

	if (_oldSwapchain) {
		device.getTable()->vkDestroySwapchainKHR(device.getDevice(), _oldSwapchain, nullptr);
		_oldSwapchain = VK_NULL_HANDLE;
	}

	if (_surface) {
		auto i = device.getInstance();
		i->vkDestroySurfaceKHR(i->getInstance(), _surface, nullptr);
		device.getTable()->vkDeviceWaitIdle(device.getDevice());
		_surface = VK_NULL_HANDLE;
	}
}

gl::ImageInfo Swapchain::getSwapchainImageInfo(const gl::SwapchainConfig &cfg) const {
	gl::ImageInfo swapchainImageInfo;
	swapchainImageInfo.format = cfg.imageFormat;
	swapchainImageInfo.flags = gl::ImageFlags::None;
	swapchainImageInfo.imageType = gl::ImageType::Image2D;
	swapchainImageInfo.extent = Extent3(cfg.extent.width, cfg.extent.height, 1);
	swapchainImageInfo.arrayLayers = gl::ArrayLayers( 1 );
	swapchainImageInfo.usage = gl::ImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	if (cfg.transfer) {
		swapchainImageInfo.usage |= gl::ImageUsage::TransferDst;
	}
	return swapchainImageInfo;
}

gl::ImageViewInfo Swapchain::getSwapchainImageViewInfo(const gl::ImageInfo &image) const {
	gl::ImageViewInfo info;
	switch (image.imageType) {
	case gl::ImageType::Image1D:
		info.type = gl::ImageViewType::ImageView1D;
		break;
	case gl::ImageType::Image2D:
		info.type = gl::ImageViewType::ImageView2D;
		break;
	case gl::ImageType::Image3D:
		info.type = gl::ImageViewType::ImageView3D;
		break;
	}

	return image.getViewInfo(info);
}

bool Swapchain::isBestPresentMode() const {
	return _presentMode == _config.presentMode;
}

Rc<SwapchainSync> Swapchain::acquireSwapchainSync(Device &dev, uint64_t idx) {
	idx = idx % FramesInFlight;
	auto &v = _sems.at(idx);
	if (!v.empty()) {
		auto ret = v.back();
		v.pop_back();
		ret->reset(_gen);
		return ret;
	}
	return Rc<SwapchainSync>::create(dev, idx, _gen);
}

void Swapchain::releaseSwapchainSync(Rc<SwapchainSync> &&ref) {
	if (!_sems.empty()) {
		_sems.at(ref->getFrameIndex() % FramesInFlight).emplace_back(move(ref));
	}
}

bool Swapchain::present(gl::Loop &loop, Rc<Image> &&image, VkImageLayout l) {
	if (!_config.transfer) {
		return false;
	}

	auto dev = (Device *)_device;

	auto &sourceImageInfo = image->getInfo();
	if (sourceImageInfo.extent.depth != 1 || sourceImageInfo.format != _config.imageFormat
			|| (sourceImageInfo.usage & gl::ImageUsage::TransferSrc) == gl::ImageUsage::None) {
		log::text("Swapchain", "Image can not be presented on swapchain");
		return false;
	}

	VkFormatProperties sourceProps;
	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(sourceImageInfo.format), &sourceProps);

	VkFormatProperties targetProps;
	dev->getInstance()->vkGetPhysicalDeviceFormatProperties(dev->getInfo().device,
			VkFormat(_config.imageFormat), &targetProps);

	VkFilter filter = VK_FILTER_NEAREST;
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

	auto frame = Rc<SwapchainPresentFrame>::alloc(loop, this, _order ++);

	frame->run(move(image), l, filter);

	return true;
}

Rc<Image> Swapchain::getImage(uint32_t i) const {
	if (i < _images.size()) {
		return _images[i];
	}
	return nullptr;
}

}
