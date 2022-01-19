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

bool Swapchain::init(const gl::View *view, Device &device, VkSurfaceKHR surface, const Rc<gl::RenderQueue> &queue) {
	if (!queue->isPresentable()) {
		return false;
	}

	_surface = surface;
	_info = device.getInstance()->getSurfaceOptions(surface, device.getPhysicalDevice());
	_sems.resize(2);

	if (_info.presentModes.empty() || _info.formats.empty()) {
		log::vtext("Vk-Error", "Presentation is not supported for :", _info.description());
		return false;
	}

	auto swapchainImageInfo = queue->getSwapchainImageInfo();

	_format = _info.formats.front();

	bool formatFound = false;
	for (const auto& availableFormat : _info.formats) {
		if (availableFormat.format == VkFormat(swapchainImageInfo->format) &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			_format = availableFormat;
			formatFound = true;
			break;
		}
	}

	if (!formatFound) {
		return false;
	}

	auto modes = getPresentModes(_info);

	_presentMode = _bestPresentMode = modes.first;
	_fastPresentMode = modes.second;

	if constexpr (s_printVkInfo) {
		Application::getInstance()->perform([this] (const Task &) {
			log::vtext("Vk-Info", "Surface info:", _info.description());
			return true;
		}, nullptr, this);
	}

	if (gl::Swapchain::init(view, queue)) {
		return createSwapchain(device, _bestPresentMode);
	}
	return false;
}

void Swapchain::invalidateFrame(gl::FrameHandle &handle) {
	auto &h = static_cast<FrameHandle &>(handle);
	h.invalidateSwapchain();
	gl::Swapchain::invalidateFrame(handle);
}

bool Swapchain::recreateSwapchain(gl::Device &idevice, gl::SwapchanCreationMode mode) {
	auto &device = dynamic_cast<Device &>(idevice);

	XL_VK_LOG("RecreateSwapChain: ", (mode == gl::SwapchanCreationMode::Best) ? "Best" : "Fast");

	auto info = device.getInstance()->getSurfaceOptions(_info.surface, device.getPhysicalDevice());

	bool found = false;
	for (const auto& availableFormat : info.formats) {
		if (availableFormat.format == _format.format && availableFormat.colorSpace == _format.colorSpace) {
			found = true;
			break;
		}
	}

	if (!found) {
		return false;
	}

	VkExtent2D extent = info.capabilities.currentExtent;
	if (extent.width == 0 || extent.height == 0) {
		return false;
	}

	if (_swapchain) {
		cleanupSwapchain(device);
		if (_nextRenderQueue) {
			_renderQueue = move(_nextRenderQueue);
			_nextRenderQueue = nullptr;
		}
	} else {
		device.getTable()->vkDeviceWaitIdle(device.getDevice());
	}

	auto modes = getPresentModes(info);

	_info = move(info);

	if (mode == gl::SwapchanCreationMode::Best) {
		return createSwapchain(device, modes.first);
	} else {
		return createSwapchain(device, modes.second);
	}
}

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

bool Swapchain::createSwapchain(Device &device, gl::PresentMode presentMode) {
	auto table = device.getTable();

	auto swapchainImageInfo = getSwapchainImageInfo();

	gl::RenderPassData *swapchainPass = nullptr;
	gl::Attachment *swapchainOut = nullptr;
	if (!_renderQueue || !_renderQueue->isCompatible(swapchainImageInfo)) {
		log::vtext("Vk-Error", "Invalid default render queue");
		return false;
	} else {
		auto out = _renderQueue->getOutput(gl::AttachmentType::SwapchainImage);
		if (out.size() == 1) {
			swapchainOut = out.front().get();
			if (auto pass = swapchainOut->getDescriptors().back()->getRenderPass()) {
				swapchainPass = pass;
			} else {
				log::vtext("Vk-Error", "Invalid default render queue");
				return false;
			}
		} else {
			log::vtext("Vk-Error", "Invalid default render queue");
			return false;
		}
	}

	uint32_t imageCount = _info.capabilities.minImageCount + 1;
	if (_info.capabilities.maxImageCount > 0 && imageCount > _info.capabilities.maxImageCount) {
		imageCount = _info.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = { };
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = _info.surface;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = VkFormat(swapchainImageInfo.format);
	swapChainCreateInfo.imageColorSpace = _format.colorSpace;
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

	swapChainCreateInfo.preTransform = _info.capabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = getVkPresentMode(presentMode);
	swapChainCreateInfo.clipped = VK_TRUE;

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

	_renderQueue->updateSwapchainInfo(swapchainImageInfo);

	if (_oldSwapchain) {
		table->vkDestroySwapchainKHR(device.getDevice(), _oldSwapchain, nullptr);
		_oldSwapchain = VK_NULL_HANDLE;
	}

	Vector<VkImage> swapchainImages;

	table->vkGetSwapchainImagesKHR(device.getDevice(), _swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	table->vkGetSwapchainImagesKHR(device.getDevice(), _swapchain, &imageCount, swapchainImages.data());

	buildAttachments(device, _renderQueue.get(), swapchainPass, move(swapchainImages));

	_presentMode = presentMode;
	_valid = true;
	return true;
}

void Swapchain::cleanupSwapchain(Device &device) {
	device.getTable()->vkDeviceWaitIdle(device.getDevice());

	if (_renderQueue) {
		for (auto &pass : _renderQueue->getPasses()) {
			for (auto &desc : pass->descriptors) {
				if (desc->getAttachment()->getType() == gl::AttachmentType::SwapchainImage) {
					pass->framebuffers.clear();
					desc->clear();
				}
			}
		}

		for (auto &attachment : _renderQueue->getAttachments()) {
			if (attachment->getType() == gl::AttachmentType::SwapchainImage) {
				attachment->clear();
			}
		}
	}

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

gl::ImageInfo Swapchain::getSwapchainImageInfo() const {
	VkExtent2D extent = _info.capabilities.currentExtent;
	gl::ImageInfo swapchainImageInfo;
	swapchainImageInfo.format = gl::ImageFormat(_format.format);
	swapchainImageInfo.flags = gl::ImageFlags::None;
	swapchainImageInfo.imageType = gl::ImageType::Image2D;
	swapchainImageInfo.extent = Extent3(extent.width, extent.height, 1);
	swapchainImageInfo.arrayLayers = gl::ArrayLayers(_info.capabilities.maxImageArrayLayers);
	swapchainImageInfo.usage = gl::ImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	return swapchainImageInfo;
}

bool Swapchain::isBestPresentMode() const {
	return _presentMode == _bestPresentMode;
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

Rc<gl::FrameHandle> Swapchain::makeFrame(gl::Loop &loop, bool readyForSubmit) {
	return Rc<FrameHandle>::create(loop, *this, *_renderQueue, _gen, readyForSubmit);
}

void Swapchain::buildAttachments(Device &device, gl::RenderQueue *queue, gl::RenderPassData *pass, const Vector<VkImage> &swapchainImages) {
	for (auto &it : queue->getAttachments()) {
		if (it->getType() == gl::AttachmentType::Buffer) {
			continue;
		}

		if (it->getType() == gl::AttachmentType::SwapchainImage) {
			if (auto image = it.cast<SwapchainAttachment>().get()) {
				Vector<Rc<Image>> images;
				for (auto &img : swapchainImages) {
					images.emplace_back(Rc<Image>::create(device, img, image->getInfo()));
				}
				image->setImages(move(images));
			} else {
				log::vtext("Vk-Error", "Unsupported swapchain attachment type");
			}
		} else {
			updateAttachment(device, it);
		}
	}

	for (auto &it : queue->getPasses()) {
		updateFramebuffer(device, it);
	}
}

void Swapchain::updateAttachment(Device &device, const Rc<gl::Attachment> &) {

}

void Swapchain::updateFramebuffer(Device &device, gl::RenderPassData *pass) {
	Extent2 extent;
	auto &lastSubpass = pass->subpasses.back();
	if (!lastSubpass.resolveImages.empty()) {
		for (auto &it : lastSubpass.resolveImages) {
			if (it) {
				extent.width = std::max(extent.width, it->getInfo().extent.width);
				extent.height = std::max(extent.height, it->getInfo().extent.height);
			}
		}
	} else if (!lastSubpass.outputImages.empty()) {
		for (auto &it : lastSubpass.outputImages) {
			if (it) {
				extent.width = std::max(extent.width, it->getInfo().extent.width);
				extent.height = std::max(extent.height, it->getInfo().extent.height);
			}
		}
	}

	size_t framebuffersCount = 0;
	for (auto &desc : pass->descriptors) {
		if (desc->getAttachment()->getType() == gl::AttachmentType::Buffer) {
			continue;
		}

		if (desc->getAttachment()->getType() == gl::AttachmentType::SwapchainImage) {
			auto image = dynamic_cast<SwapchainAttachment *>(desc->getAttachment());
			auto imageDesc = dynamic_cast<SwapchainAttachmentDescriptor *>(desc);
			if (image && imageDesc) {
				framebuffersCount = std::max(framebuffersCount, image->getImages().size());

				if (!imageDesc->getImageViews().empty() && imageDesc->getImageViews().size() == image->getImages().size()) {
					Vector<Rc<ImageView>> imageViews;
					size_t idx = 0;
					for (auto &it : imageDesc->getImageViews()) {
						if (it->getImage() == image->getImages().at(idx)) {
							imageViews.emplace_back(it.get());
						} else {
							auto iv = Rc<ImageView>::create(device, *imageDesc, image->getImages().at(idx).cast<Image>());
							imageViews.emplace_back(it.get());
						}
						++ idx;
					}
					imageDesc->setImageViews(move(imageViews));
				} else {
					imageDesc->clear();
					Vector<Rc<ImageView>> imageViews;
					for (auto &it : image->getImages()) {
						auto iv = Rc<ImageView>::create(device, *imageDesc, it.cast<Image>());
						imageViews.emplace_back(iv.get());
					}
					imageDesc->setImageViews(move(imageViews));
				}
			} else {
				log::vtext("Vk-Error", "Unsupported swapchain attachment type");
			}
		} else {
			framebuffersCount = std::max(framebuffersCount, size_t(1));
			auto image = dynamic_cast<ImageAttachment *>(desc->getAttachment());
			auto imageDesc = dynamic_cast<ImageAttachmentDescriptor *>(desc);
			if (image && imageDesc) {
				if (auto view = imageDesc->getImageView()) {
					if (view->getImage() != image->getImage()) {
						auto iv = Rc<ImageView>::create(device, *imageDesc, image->getImage().cast<Image>());
						imageDesc->setImageView(iv);
					}
				} else {
					auto iv = Rc<ImageView>::create(device, *imageDesc, image->getImage().cast<Image>());
					imageDesc->setImageView(iv);
				}
			}
		}
	}

	pass->framebuffers.clear();
	for (size_t i = 0; i < framebuffersCount; ++ i) {
		Vector<VkImageView> imageViews;
		for (auto &desc : pass->descriptors) {
			switch (desc->getAttachment()->getType()) {
			case gl::AttachmentType::Buffer:
			case gl::AttachmentType::Generic:
				break;
			case gl::AttachmentType::Image:
				imageViews.emplace_back(((ImageAttachmentDescriptor *)desc)->getImageView().cast<ImageView>()->getImageView());
				break;
			case gl::AttachmentType::SwapchainImage:
				imageViews.emplace_back(((SwapchainAttachmentDescriptor *)desc)->getImageViews()
						[i % ((SwapchainAttachmentDescriptor *)desc)->getImageViews().size()].cast<ImageView>()->getImageView());
				break;
			}
		}
		auto fb = Rc<Framebuffer>::create(device, pass->impl.cast<RenderPassImpl>()->getRenderPass(), imageViews, extent);
		pass->framebuffers.emplace_back(fb.get());
	}
}

Pair<gl::PresentMode, gl::PresentMode> Swapchain::getPresentModes(const SurfaceInfo &info) const {
	gl::PresentMode fast, best;
	 // best available mode will be first
	fast = best = info.presentModes.front();

	// check for immediate mode
	for (auto &it : info.presentModes) {
		if (it == gl::PresentMode::Immediate) {
			fast = it;
			break;
		}
	}

	return pair(best, fast);
}

}
