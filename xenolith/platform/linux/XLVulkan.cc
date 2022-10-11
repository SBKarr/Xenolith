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

#include "XLPlatform.h"

#if LINUX

#include "XLApplication.h"
#include "XLPlatformLinuxWayland.h"
#include "XLPlatformLinuxXcb.h"
#include "XLVkInstance.h"

#include <dlfcn.h>

namespace stappler::xenolith::platform::graphic {

struct FunctionTable : public vk::LoaderTable {
	using LoaderTable::LoaderTable;

	operator bool () const {
		return vkGetInstanceProcAddr != nullptr
			&& vkCreateInstance != nullptr
			&& vkEnumerateInstanceExtensionProperties != nullptr
			&& vkEnumerateInstanceLayerProperties != nullptr;
	}
};

static uint32_t s_InstanceVersion = 0;
static Vector<VkLayerProperties> s_InstanceAvailableLayers;
static Vector<VkExtensionProperties> s_InstanceAvailableExtensions;

Rc<gl::Instance> createInstance(Application *app) {
	auto handle = ::dlopen("libvulkan.so.1", RTLD_LAZY | RTLD_LOCAL);
	if (!handle) {
		stappler::log::text("Vk", "Fail to open libvulkan.so.1");
		return nullptr;
	}

	auto getInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(handle, "vkGetInstanceProcAddr");
	if (!getInstanceProcAddr) {
		return nullptr;
	}

	FunctionTable table(getInstanceProcAddr);

	if (!table) {
		::dlclose(handle);
		return nullptr;
	}

	if (table.vkEnumerateInstanceVersion) {
		table.vkEnumerateInstanceVersion(&s_InstanceVersion);
	} else {
		s_InstanceVersion = VK_API_VERSION_1_0;
	}

	uint32_t targetVersion = VK_API_VERSION_1_0;
	if (s_InstanceVersion >= VK_API_VERSION_1_2) {
		targetVersion = VK_API_VERSION_1_2;
	} else if (s_InstanceVersion >= VK_API_VERSION_1_1) {
		targetVersion = VK_API_VERSION_1_1;
	}

    uint32_t layerCount = 0;
    table.vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	s_InstanceAvailableLayers.resize(layerCount);
	table.vkEnumerateInstanceLayerProperties(&layerCount, s_InstanceAvailableLayers.data());

    uint32_t extensionCount = 0;
    table.vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    s_InstanceAvailableExtensions.resize(extensionCount);
    table.vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, s_InstanceAvailableExtensions.data());

	Vector<const char *> enableLayers;

	if constexpr (vk::s_enableValidationLayers) {
#if DEBUG
		if (app->getData().validation) {
			for (const char *layerName : vk::s_validationLayers) {
				bool layerFound = false;

				for (const auto &layerProperties : s_InstanceAvailableLayers) {
					if (strcmp(layerName, layerProperties.layerName) == 0) {
						enableLayers.emplace_back(layerName);
						layerFound = true;
						break;
					}
				}

				if (!layerFound) {
					log::format("Vk", "Required validation layer not found: %s", layerName);
					return nullptr;
				}
			}
		}
#endif
	}

	for (const auto &layerProperties : s_InstanceAvailableLayers) {
		if (app->getData().renderdoc && strcmp("VK_LAYER_RENDERDOC_Capture", layerProperties.layerName) == 0) {
			enableLayers.emplace_back(layerProperties.layerName);
		}
	}

	const char *surfaceExt = nullptr;
	const char *debugExt = nullptr;

	SurfaceType osSurfaceType = SurfaceType::None;
	auto xcbLib = Rc<platform::XcbLibrary>::create();
	if (xcbLib) {
		osSurfaceType |= SurfaceType::XCB;
	}

	auto waylandLib = Rc<platform::WaylandLibrary>::create();
	if (waylandLib) {
		osSurfaceType |= SurfaceType::Wayland;
	}

	SurfaceType surfaceType = SurfaceType::None;
	Vector<const char*> requiredExtensions;
	Vector<StringView> enabledOptionals;

	for (auto &extension : s_InstanceAvailableExtensions) {
		if constexpr (vk::s_enableValidationLayers) {
			if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, extension.extensionName) == 0) {
				requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				debugExt = extension.extensionName;
				continue;
			}
		}
		if (strcmp(VK_KHR_SURFACE_EXTENSION_NAME, extension.extensionName) == 0) {
			surfaceExt = extension.extensionName;
			requiredExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		} else if (strcmp(VK_KHR_XCB_SURFACE_EXTENSION_NAME, extension.extensionName) == 0
				&& (osSurfaceType & SurfaceType::XCB) != SurfaceType::None) {
			surfaceType |= SurfaceType::XCB;
			requiredExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
		} else if (strcmp(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, extension.extensionName) == 0
				&& (osSurfaceType & SurfaceType::Wayland) != SurfaceType::None) {
			surfaceType |= SurfaceType::Wayland;
			requiredExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
		} else {
			for (auto &it : vk::s_optionalExtension) {
				if (it) {
					if (strcmp(it, extension.extensionName) == 0) {
						requiredExtensions.emplace_back(it);
						enabledOptionals.emplace_back(StringView(it));
					}
				}
			}
		}
	}

	bool completeExt = true;

	for (auto &it : vk::s_requiredExtension) {
		if (it) {
			bool found = false;
			for (auto &extension : s_InstanceAvailableExtensions) {
				if (strcmp(it, extension.extensionName) == 0) {
					found = true;
					requiredExtensions.emplace_back(it);
				}
			}
			if (!found) {
				log::format("Vk", "Required extension not found: %s", it);
				completeExt = false;
			}
		}
	}

	if (!surfaceExt || surfaceType == SurfaceType::None) {
		log::format("Vk", "Required extension not found: %s", VK_KHR_SURFACE_EXTENSION_NAME);
		completeExt = false;
	}

	if constexpr (vk::s_enableValidationLayers) {
		if (!debugExt) {
			log::format("Vk", "Required extension not found: %s", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			completeExt = false;
		}
	}

	if (!surfaceExt) {
		log::format("Vk", "Required extension not found: %s", VK_KHR_SURFACE_EXTENSION_NAME);
		completeExt = false;
	}

	if (!completeExt) {
		log::text("Vk", "Not all required extensions found, fail to create VkInstance");
		::dlclose(handle);
		return nullptr;
	}

	VkInstance instance;

	auto name = app->getData().bundleName;
	auto version = app->getData().applicationVersion;

	uint32_t versionArgs[3] = { 0 };
	size_t i = 0;
	StringView(version).split<StringView::Chars<'.'>>([&] (StringView v) {
		v.readInteger(10).unwrap([&] (int64_t val) {
			if (i < 3) {
				versionArgs[i] = uint32_t(val);
			}
			++ i;
		});
	});

	VkApplicationInfo appInfo{}; vk::sanitizeVkStruct(appInfo);
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = name.data();
	appInfo.applicationVersion = VK_MAKE_VERSION(versionArgs[0], versionArgs[1], versionArgs[2]);
	appInfo.pEngineName = version::_name();
	appInfo.engineVersion = version::_version();
	appInfo.apiVersion = targetVersion;

	VkInstanceCreateInfo createInfo{}; vk::sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.pApplicationInfo = &appInfo;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	enum VkResult ret = VK_SUCCESS;
	if constexpr (vk::s_enableValidationLayers) {
#if VK_DEBUG_LOG
	    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
	    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	    debugCreateInfo.pfnUserCallback = vk::s_debugCallback;
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
#endif
	} else {
		createInfo.pNext = nullptr;
	}

	createInfo.enabledLayerCount = enableLayers.size();
	createInfo.ppEnabledLayerNames = enableLayers.data();
	ret = table.vkCreateInstance(&createInfo, nullptr, &instance);

	if (ret != VK_SUCCESS) {
		log::text("Vk", "Fail to create Vulkan instance");
		return nullptr;
	}

	auto vkInstance = Rc<vk::Instance>::alloc(instance, table.vkGetInstanceProcAddr, targetVersion, move(enabledOptionals),
			[handle] {
		::dlclose(handle);
	}, [surfaceType, xcbLib, waylandLib] (const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx) {
		uint32_t ret = 0;
		if ((surfaceType & SurfaceType::Wayland) != SurfaceType::None) {
			auto display = waylandLib->getActiveConnection().display;
			auto supports = instance->vkGetPhysicalDeviceWaylandPresentationSupportKHR(device, queueIdx, display);
			if (supports) {
				ret |= toInt(SurfaceType::Wayland);
			}
		}
		if ((surfaceType & SurfaceType::XCB) != SurfaceType::None) {
			auto conn = xcbLib->getActiveConnection();
			auto supports = instance->vkGetPhysicalDeviceXcbPresentationSupportKHR(device, queueIdx, conn.connection, conn.screen->root_visual);
			if (supports) {
				ret |= toInt(SurfaceType::XCB);
			}
		}
		return ret;
	});

	if constexpr (vk::s_printVkInfo) {
		app->perform([vkInstance] (const Task &) {
			StringStream out;
			out << "\n\tLayers:\n";
			for (const auto &layerProperties : s_InstanceAvailableLayers) {
				out << "\t\t" << layerProperties.layerName << " ("
						<< gl::Instance::getVersionDescription(layerProperties.specVersion) << "/"
						<< gl::Instance::getVersionDescription(layerProperties.implementationVersion)
						<< ")\t - " << layerProperties.description << "\n";
			}

			out << "\tExtension:\n";
			for (const auto &extension : s_InstanceAvailableExtensions) {
				out << "\t\t" << extension.extensionName << ": " << vk::Instance::getVersionDescription(extension.specVersion) << "\n";
			}

			vkInstance->printDevicesInfo(out);

			log::text("Vk-Info", out.str());
			return true;
		});
	}

	return vkInstance;
}

}

#endif
