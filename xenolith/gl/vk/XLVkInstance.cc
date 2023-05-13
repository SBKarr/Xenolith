/**
 Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLVkInstance.h"
#include "XLPlatform.h"
#include "XLGlLoop.h"
#include "XLGlDevice.h"

namespace stappler::xenolith::vk {

#if VK_DEBUG_LOG

static VkResult s_createDebugUtilsMessengerEXT(VkInstance instance, const PFN_vkGetInstanceProcAddr getInstanceProcAddr,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

VKAPI_ATTR VkBool32 VKAPI_CALL s_debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	if (pCallbackData->pMessageIdName && strcmp(pCallbackData->pMessageIdName, "VUID-VkSwapchainCreateInfoKHR-imageExtent-01274") == 0) {
		// this is normal for multithreaded engine
		messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	}
	if (pCallbackData->pMessageIdName && strcmp(pCallbackData->pMessageIdName, "Loader Message") == 0) {
		//if (messageSeverity < XL_VK_MIN_LOADER_MESSAGE_SEVERITY) {
		//	return VK_FALSE;
		//}

		if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
			if (StringView(pCallbackData->pMessage).starts_with("Instance Extension: ")
					|| StringView(pCallbackData->pMessage).starts_with("Device Extension: ")) {
				return VK_FALSE;
			}
			log::vtext("Vk-Validation-Verbose", "[", pCallbackData->pMessageIdName, "] ", pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			log::vtext("Vk-Validation-Info", "[", pCallbackData->pMessageIdName, "] ", pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			log::vtext("Vk-Validation-Warning", "[", pCallbackData->pMessageIdName, "] ", pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			log::vtext("Vk-Validation-Error", "[", pCallbackData->pMessageIdName, "] ", pCallbackData->pMessage);
		}
		return VK_FALSE;
	} else {
		if (messageSeverity < XL_VK_MIN_MESSAGE_SEVERITY) {
			return VK_FALSE;
		}

		if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
			if (StringView(pCallbackData->pMessage).starts_with("Device Extension: ")) {
				return VK_FALSE;
			}
			log::vtext("Vk-Validation-Verbose", "[",
				pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)",
				"] ", pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			log::vtext("Vk-Validation-Info", "[",
				pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)",
				"] ", pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			log::vtext("Vk-Validation-Warning", "[",
				pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)",
				"] ", pCallbackData->pMessage);
		} else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			log::vtext("Vk-Validation-Error", "[",
				pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "(null)",
				"] ", pCallbackData->pMessage);
		}
		return VK_FALSE;
	}
}

#endif

Instance::Instance(VkInstance inst, const PFN_vkGetInstanceProcAddr getInstanceProcAddr, uint32_t targetVersion,
		Vector<StringView> &&optionals, TerminateCallback &&terminate, PresentSupportCallback &&present)
: gl::Instance(move(terminate)), InstanceTable(getInstanceProcAddr, inst),  _instance(inst)
, _version(targetVersion)
, _optionals(move(optionals))
, _checkPresentSupport(move(present)) {
	if constexpr (s_enableValidationLayers) {
#if VK_DEBUG_LOG
		if (Application::getInstance()->getData().validation) {
			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { };
			debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

			debugCreateInfo.pfnUserCallback = s_debugCallback;

			if (s_createDebugUtilsMessengerEXT(_instance, vkGetInstanceProcAddr, &debugCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
				log::text("Vk", "failed to set up debug messenger!");
			}
		}
#endif
	}

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);

	if (deviceCount) {
		Vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

		for (auto &device : devices) {
			auto &it = _devices.emplace_back(getDeviceInfo(device));

			_availableDevices.emplace_back(gl::DeviceProperties{
				String((const char *)it.properties.device10.properties.deviceName),
				it.properties.device10.properties.apiVersion,
				it.properties.device10.properties.driverVersion,
				it.supportsPresentation()
			});
		}
	}
}

Instance::~Instance() {
	if constexpr (s_enableValidationLayers) {
#if VK_DEBUG_LOG
		if (debugMessenger != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(_instance, debugMessenger, nullptr);
		}
#endif
	}
	vkDestroyInstance(_instance, nullptr);
}

Rc<gl::Loop> Instance::makeLoop(Application *app, uint32_t deviceIndex) const {
	return Rc<vk::Loop>::create(app, Rc<Instance>(const_cast<Instance *>(this)), deviceIndex);
}

Rc<Device> Instance::makeDevice(uint32_t deviceIndex) const {
	if (deviceIndex == maxOf<uint32_t>()) {
		for (auto &it : _devices) {
			if (it.supportsPresentation()) {
				auto requiredFeatures = DeviceInfo::Features::getOptional();
				requiredFeatures.enableFromFeatures(DeviceInfo::Features::getRequired());
				requiredFeatures.disableFromFeatures(it.features);
				requiredFeatures.flags = it.features.flags;

				if (it.features.canEnable(requiredFeatures, it.properties.device10.properties.apiVersion)) {
					return Rc<vk::Device>::create(this, DeviceInfo(it), requiredFeatures);
				}
			}
		}
	} else if (deviceIndex < _devices.size()) {
		if (_devices[deviceIndex].supportsPresentation()) {
			auto requiredFeatures = DeviceInfo::Features::getOptional();
			requiredFeatures.enableFromFeatures(DeviceInfo::Features::getRequired());
			requiredFeatures.disableFromFeatures(_devices[deviceIndex].features);
			requiredFeatures.flags = _devices[deviceIndex].features.flags;

			if (_devices[deviceIndex].features.canEnable(requiredFeatures, _devices[deviceIndex].properties.device10.properties.apiVersion)) {
				return Rc<vk::Device>::create(this, DeviceInfo(_devices[deviceIndex]), requiredFeatures);
			}
		}
	}
	return nullptr;
}

static gl::PresentMode getGlPresentMode(VkPresentModeKHR presentMode) {
	switch (presentMode) {
	case VK_PRESENT_MODE_IMMEDIATE_KHR: return gl::PresentMode::Immediate; break;
	case VK_PRESENT_MODE_MAILBOX_KHR: return gl::PresentMode::Mailbox; break;
	case VK_PRESENT_MODE_FIFO_KHR: return gl::PresentMode::Fifo; break;
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return gl::PresentMode::FifoRelaxed; break;
	default: return gl::PresentMode::Unsupported; break;
	}
}

gl::SurfaceInfo Instance::getSurfaceOptions(VkSurfaceKHR surface, VkPhysicalDevice device) const {
	gl::SurfaceInfo ret;

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (formatCount != 0) {
		Vector<VkSurfaceFormatKHR> formats; formats.resize(formatCount);

		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());

		ret.formats.reserve(formatCount);
		for (auto &it : formats) {
			ret.formats.emplace_back(gl::ImageFormat(it.format), gl::ColorSpace(it.colorSpace));
		}
	}

	if (presentModeCount != 0) {
		ret.presentModes.reserve(presentModeCount);
		Vector<VkPresentModeKHR> modes; modes.resize(presentModeCount);

		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, modes.data());

		for (auto &it : modes) {
			ret.presentModes.emplace_back(getGlPresentMode(it));
		}

		std::sort(ret.presentModes.begin(), ret.presentModes.end(), [&] (gl::PresentMode l, gl::PresentMode r) {
			return toInt(l) > toInt(r);
		});
	}

	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &caps);

	ret.minImageCount = caps.minImageCount;
	ret.maxImageCount = caps.maxImageCount;
	ret.currentExtent = Extent2(caps.currentExtent.width, caps.currentExtent.height);
	ret.minImageExtent = Extent2(caps.minImageExtent.width, caps.minImageExtent.height);
	ret.maxImageExtent = Extent2(caps.maxImageExtent.width, caps.maxImageExtent.height);
	ret.maxImageArrayLayers = caps.maxImageArrayLayers;
	ret.supportedTransforms = gl::SurfaceTransformFlags(caps.supportedTransforms);
	ret.currentTransform = gl::SurfaceTransformFlags(caps.currentTransform);
	ret.supportedCompositeAlpha = gl::CompositeAlphaFlags(caps.supportedCompositeAlpha);
	ret.supportedUsageFlags = gl::ImageUsage(caps.supportedUsageFlags);
	return ret;
}

VkExtent2D Instance::getSurfaceExtent(VkSurfaceKHR surface, VkPhysicalDevice device) const {
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
	return capabilities.currentExtent;
}

VkInstance Instance::getInstance() const {
	return _instance;
}

void Instance::printDevicesInfo(std::ostream &out) const {
	out << "\n";

	auto getDeviceTypeString = [&] (VkPhysicalDeviceType type) -> const char * {
		switch (type) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual GPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU"; break;
		default: return "Other"; break;
		}
		return "Other";
	};

	for (auto &device : _devices) {
		out << "\tDevice: " << device.device << " " << getDeviceTypeString(device.properties.device10.properties.deviceType)
				<< ": " << device.properties.device10.properties.deviceName
				<< " (API: " << getVersionDescription(device.properties.device10.properties.apiVersion)
				<< ", Driver: " << getVersionDescription(device.properties.device10.properties.driverVersion) << ")\n";

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device.device, &queueFamilyCount, nullptr);

		Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device.device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const VkQueueFamilyProperties& queueFamily : queueFamilies) {
			bool empty = true;
			out << "\t\t[" << i << "] Queue family; Flags: ";
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				if (!empty) { out << ", "; } else { empty = false; }
				out << "Graphics";
			}
			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				if (!empty) { out << ", "; } else { empty = false; }
				out << "Compute";
			}
			if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				if (!empty) { out << ", "; } else { empty = false; }
				out << "Transfer";
			}
			if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
				if (!empty) { out << ", "; } else { empty = false; }
				out << "SparseBinding";
			}
			if (queueFamily.queueFlags & VK_QUEUE_PROTECTED_BIT) {
				if (!empty) { out << ", "; } else { empty = false; }
				out << "Protected";
			}

			VkBool32 presentSupport = _checkPresentSupport(this, device.device, i);
			if (presentSupport) {
				if (!empty) { out << ", "; } else { empty = false; }
				out << "Present";
			}

			out << "; Count: " << queueFamily.queueCount << "\n";
			i++;
		}
		out << device.description();
	}
}

void Instance::getDeviceFeatures(const VkPhysicalDevice &device, DeviceInfo::Features &features, ExtensionFlags flags, uint32_t api) const {
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if ((flags & ExtensionFlags::Portability) != ExtensionFlags::None) {
		features.devicePortability.pNext = next;
		next = &features.devicePortability;
	}
#endif
	features.flags = flags;
	if (api >= VK_API_VERSION_1_3) {
		features.device13.pNext = next;
		features.device12.pNext = &features.device13;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateFrom13();
	} else if (api >= VK_API_VERSION_1_2) {
		features.device12.pNext = next;
		features.device11.pNext = &features.device12;
		features.device10.pNext = &features.device11;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateFrom12();
	} else {
		if ((flags & ExtensionFlags::Storage16Bit) != ExtensionFlags::None) {
			features.device16bitStorage.pNext = next;
			next = &features.device16bitStorage;
		}
		if ((flags & ExtensionFlags::Storage8Bit) != ExtensionFlags::None) {
			features.device8bitStorage.pNext = next;
			next = &features.device8bitStorage;
		}
		if ((flags & ExtensionFlags::ShaderFloat16) != ExtensionFlags::None || (flags & ExtensionFlags::ShaderInt8) != ExtensionFlags::None) {
			features.deviceShaderFloat16Int8.pNext = next;
			next = &features.deviceShaderFloat16Int8;
		}
		if ((flags & ExtensionFlags::DescriptorIndexing) != ExtensionFlags::None) {
			features.deviceDescriptorIndexing.pNext = next;
			next = &features.deviceDescriptorIndexing;
		}
		if ((flags & ExtensionFlags::DeviceAddress) != ExtensionFlags::None) {
			features.deviceBufferDeviceAddress.pNext = next;
			next = &features.deviceBufferDeviceAddress;
		}
		features.device10.pNext = next;

		if (vkGetPhysicalDeviceFeatures2) {
			vkGetPhysicalDeviceFeatures2(device, &features.device10);
		} else if (vkGetPhysicalDeviceFeatures2KHR) {
			vkGetPhysicalDeviceFeatures2KHR(device, &features.device10);
		} else {
			vkGetPhysicalDeviceFeatures(device, &features.device10.features);
		}

		features.updateTo12(true);
	}
}

void Instance::getDeviceProperties(const VkPhysicalDevice &device, DeviceInfo::Properties &properties, ExtensionFlags flags, uint32_t api) const {
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if ((flags & ExtensionFlags::Portability) != ExtensionFlags::None) {
		properties.devicePortability.pNext = next;
		next = &properties.devicePortability;
	}
#endif
	if ((flags & ExtensionFlags::Maintenance3) != ExtensionFlags::None) {
		properties.deviceMaintenance3.pNext = next;
		next = &properties.deviceMaintenance3;
	}
	if ((flags & ExtensionFlags::DescriptorIndexing) != ExtensionFlags::None) {
		properties.deviceDescriptorIndexing.pNext = next;
		next = &properties.deviceDescriptorIndexing;
	}

	properties.device10.pNext = next;

	if (vkGetPhysicalDeviceProperties2) {
		vkGetPhysicalDeviceProperties2(device, &properties.device10);
	} else if (vkGetPhysicalDeviceProperties2KHR) {
		vkGetPhysicalDeviceProperties2KHR(device, &properties.device10);
	} else {
		vkGetPhysicalDeviceProperties(device, &properties.device10.properties);
	}
}

DeviceInfo Instance::getDeviceInfo(VkPhysicalDevice device) const {
	DeviceInfo ret;
	uint32_t graphicsFamily = maxOf<uint32_t>();
	uint32_t presentFamily = maxOf<uint32_t>();
	uint32_t transferFamily = maxOf<uint32_t>();
	uint32_t computeFamily = maxOf<uint32_t>();

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	Vector<DeviceInfo::QueueFamilyInfo> queueInfo(queueFamilyCount);
	Vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const VkQueueFamilyProperties &queueFamily : queueFamilies) {
		auto presentSupport = _checkPresentSupport(this, device, i);

		queueInfo[i].index = i;
		queueInfo[i].ops = getQueueOperations(queueFamily.queueFlags, presentSupport);
		queueInfo[i].count = queueFamily.queueCount;
		queueInfo[i].used = 0;
		queueInfo[i].minImageTransferGranularity = queueFamily.minImageTransferGranularity;
		queueInfo[i].presentSurfaceMask = presentSupport;

		if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphicsFamily == maxOf<uint32_t>()) {
			graphicsFamily = i;
		}

		if ((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) && transferFamily == maxOf<uint32_t>()) {
			transferFamily = i;
		}

		if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && computeFamily == maxOf<uint32_t>()) {
			computeFamily = i;
		}

		if (presentSupport != 0 && presentFamily == maxOf<uint32_t>()) {
			presentFamily = i;
		}

		i ++;
	}

	// try to select different families for transfer and compute (for more concurrency)
	if (computeFamily == graphicsFamily) {
		for (auto &it : queueInfo) {
			if (it.index != graphicsFamily && ((it.ops & QueueOperations::Compute) != QueueOperations::None)) {
				computeFamily = it.index;
			}
		}
	}

	if (transferFamily == computeFamily || transferFamily == graphicsFamily) {
		for (auto &it : queueInfo) {
			if (it.index != graphicsFamily && it.index != computeFamily && ((it.ops & QueueOperations::Transfer) != QueueOperations::None)) {
				transferFamily = it.index;
			}
		}
		if (transferFamily == computeFamily || transferFamily == graphicsFamily) {
			if (queueInfo[computeFamily].count >= queueInfo[graphicsFamily].count) {
				transferFamily = computeFamily;
			} else {
				transferFamily = graphicsFamily;
			}
		}
	}

	// try to map present with graphics
	if (presentFamily != graphicsFamily) {
		if ((queueInfo[graphicsFamily].ops & QueueOperations::Present) != QueueOperations::None) {
			presentFamily = graphicsFamily;
		}
	}

	// fallback when Transfer or Compute is not defined
	if (transferFamily == maxOf<uint32_t>()) {
		transferFamily = graphicsFamily;
		queueInfo[transferFamily].ops |= QueueOperations::Transfer;
	}

	if (computeFamily == maxOf<uint32_t>()) {
		computeFamily = graphicsFamily;
	}

	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	Vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	// we need only API version for now
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	// find required device extensions
	bool notFound = false;
	for (auto &extensionName : s_requiredDeviceExtensions) {
		if (!extensionName) {
			break;
		}

		if (isPromotedExtension(deviceProperties.apiVersion, extensionName)) {
			continue;
		}

		bool found = false;
		for (auto &extension : availableExtensions) {
			if (strcmp(extensionName, extension.extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (!found) {
			if constexpr (s_printVkInfo) {
				log::format("Vk-Info", "Required device extension not found: %s", extensionName);
			}
			notFound = true;
			break;
		}
	}

	if (notFound) {
		ret.requiredExtensionsExists = false;
	} else {
		ret.requiredExtensionsExists = true;
	}

	// check for optionals
	ExtensionFlags extensionFlags = ExtensionFlags::None;
	Vector<StringView> enabledOptionals;
	Vector<StringView> promotedOptionals;
	for (auto &extensionName : s_optionalDeviceExtensions) {
		if (!extensionName) {
			break;
		}

		checkIfExtensionAvailable(deviceProperties.apiVersion,
				extensionName, availableExtensions, enabledOptionals, promotedOptionals, extensionFlags);
	}

	ret.device = device;
	ret.graphicsFamily = queueInfo[graphicsFamily];
	ret.presentFamily = (presentFamily == maxOf<uint32_t>()) ? DeviceInfo::QueueFamilyInfo() : queueInfo[presentFamily];
	ret.transferFamily = queueInfo[transferFamily];
	ret.computeFamily = queueInfo[computeFamily];
	ret.optionalExtensions = move(enabledOptionals);
	ret.promotedExtensions = move(promotedOptionals);

	getDeviceProperties(device, ret.properties, extensionFlags, deviceProperties.apiVersion);
	getDeviceFeatures(device, ret.features, extensionFlags, deviceProperties.apiVersion);

	auto requiredFeatures = DeviceInfo::Features::getRequired();
	ret.requiredFeaturesExists = ret.features.canEnable(requiredFeatures, deviceProperties.apiVersion);

	return ret;
}

}
