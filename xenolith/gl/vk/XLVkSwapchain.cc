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

#include "XLVkSwapchain.h"
#include "XLVkInstance.h"
#include "XLVkDevice.h"
#include "XLVkFramebuffer.h"
#include "XLGlRenderQueue.h"

namespace stappler::xenolith::vk {

Swapchain::~Swapchain() {
	_defaultRenderQueue = nullptr;
}

bool Swapchain::init(Device &device) {
	_info = device.getInstance()->getSurfaceOptions(device.getSurface(), device.getPhysicalDevice());

	if (_info.presentModes.empty() || _info.formats.empty()) {
		log::vtext("Vk-Error", "Presentation is not supported for :", _info.description());
		return false;
	}

	_format = _info.formats.front();

	for (const auto& availableFormat : _info.formats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			_format = availableFormat;
			break;
		}
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

	return true;
}

bool Swapchain::recreateSwapChain(Device &device, const Rc<gl::Loop> &loop, thread::TaskQueue &queue, bool resize) {
	XL_VK_LOG("RecreateSwapChain: ", resize ? "Fast": "Best");

	auto info = device.getInstance()->getSurfaceOptions(device.getSurface(), device.getPhysicalDevice());

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

	if (_swapChain) {
		cleanupSwapChain(device);
	} else {
		device.getTable()->vkDeviceWaitIdle(device.getDevice());
	}

	auto modes = getPresentModes(info);

	if (resize) {
		return createSwapChain(device, loop, queue, move(info), modes.second);
	} else {
		return createSwapChain(device, loop, queue, move(info), modes.first);
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

bool Swapchain::createSwapChain(Device &device, const Rc<gl::Loop> &loop, thread::TaskQueue &queue, SurfaceInfo &&info, gl::PresentMode presentMode) {
	auto table = device.getTable();

	VkExtent2D extent = info.capabilities.currentExtent;
	gl::ImageInfo swapchainImageInfo;
	swapchainImageInfo.format = gl::ImageFormat(_format.format);
	swapchainImageInfo.flags = gl::ImageFlags::None;
	swapchainImageInfo.imageType = gl::ImageType::Image2D;
	swapchainImageInfo.extent = Extent3(extent.width, extent.height, 1);
	swapchainImageInfo.arrayLayers = gl::ArrayLayers(info.capabilities.maxImageArrayLayers);
	swapchainImageInfo.usage = gl::ImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	gl::RenderPassData *swapchainPass = nullptr;
	gl::Attachment *swapchainOut = nullptr;
	if (!_defaultRenderQueue || !_defaultRenderQueue->isCompatible(swapchainImageInfo)) {
		auto q = device.createDefaultRenderQueue(loop, queue, swapchainImageInfo);
		if (!q->isCompatible(swapchainImageInfo)) {
			log::vtext("Vk-Error", "Invalid default render queue");
			return false;
		}

		auto out = q->getOutput(gl::AttachmentType::SwapchainImage);
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
		_defaultRenderQueue = q;
	} else {
		_defaultRenderQueue->updateSwapchainInfo(swapchainImageInfo);
	}

	if (swapchainImageInfo.format != gl::ImageFormat(_format.format)
			|| swapchainImageInfo.flags != gl::ImageFlags::None
			|| swapchainImageInfo.imageType != gl::ImageType::Image2D
			|| swapchainImageInfo.extent != Extent3(extent.width, extent.height, 1)
			|| swapchainImageInfo.arrayLayers < gl::ArrayLayers(1)
			|| swapchainImageInfo.arrayLayers > gl::ArrayLayers(info.capabilities.maxImageArrayLayers)
			|| swapchainImageInfo.samples != gl::SampleCount::X1
			|| swapchainImageInfo.tiling != gl::ImageTiling::Optimal) {
		log::vtext("Vk-Error", "Invalid swapchain image info:", swapchainImageInfo.description());
		return false;
	}

	uint32_t imageCount = info.capabilities.minImageCount + 1;
	if (info.capabilities.maxImageCount > 0 && imageCount > info.capabilities.maxImageCount) {
		imageCount = info.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = { };
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = device.getSurface();
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = VkFormat(swapchainImageInfo.format);
	swapChainCreateInfo.imageColorSpace = _format.colorSpace;
	swapChainCreateInfo.imageExtent = extent;
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

	swapChainCreateInfo.preTransform = info.capabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = getVkPresentMode(presentMode);
	swapChainCreateInfo.clipped = VK_TRUE;

	if (_oldSwapChain) {
		swapChainCreateInfo.oldSwapchain = _oldSwapChain;
	} else {
		swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	}

	if (table->vkCreateSwapchainKHR(device.getDevice(), &swapChainCreateInfo, nullptr, &_swapChain) != VK_SUCCESS) {
		return false;
	}

	if (_oldSwapChain) {
		table->vkDestroySwapchainKHR(device.getDevice(), _oldSwapChain, nullptr);
		_oldSwapChain = VK_NULL_HANDLE;
	}

	Vector<VkImage> swapchainImages;

	table->vkGetSwapchainImagesKHR(device.getDevice(), _swapChain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	table->vkGetSwapchainImagesKHR(device.getDevice(), _swapChain, &imageCount, swapchainImages.data());

	buildAttachments(device, _defaultRenderQueue.get(), swapchainPass, move(swapchainImages));

	_presentMode = presentMode;
	_info = move(info);

	return true;
}

void Swapchain::cleanupSwapChain(Device &device) {
	device.getTable()->vkDeviceWaitIdle(device.getDevice());

	for (auto &pass : _defaultRenderQueue->getPasses()) {
		for (auto &desc : pass->descriptors) {
			if (desc->getAttachment()->getType() == gl::AttachmentType::SwapchainImage) {
				pass->framebuffers.clear();
				desc->clear();
			}
		}
	}

	for (auto &attachment : _defaultRenderQueue->getAttachments()) {
		if (attachment->getType() == gl::AttachmentType::SwapchainImage) {
			attachment->clear();
		}
	}

	if (_swapChain) {
		_oldSwapChain = _swapChain;
		_swapChain = VK_NULL_HANDLE;
	}
}

void Swapchain::invalidate(Device &device) {
	cleanupSwapChain(device);

	if (_oldSwapChain) {
		device.getTable()->vkDestroySwapchainKHR(device.getDevice(), _oldSwapChain, nullptr);
		_oldSwapChain = VK_NULL_HANDLE;
	}
}

bool Swapchain::isBestPresentMode() const {
	return _presentMode == _bestPresentMode;
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
	auto lastSubpass = pass->subpasses.back();
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
