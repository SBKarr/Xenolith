/**
Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef COMPONENTS_XENOLITH_GL_XLVKINSTANCE_H_
#define COMPONENTS_XENOLITH_GL_XLVKINSTANCE_H_

#include "XLGlInstance.h"
#include "XLGlSwapchain.h"
#include "XLVkInfo.h"

namespace stappler::xenolith::vk {

#if VK_DEBUG_LOG

VKAPI_ATTR VkBool32 VKAPI_CALL s_debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

#endif

class Instance : public gl::Instance, public InstanceTable {
public:
	using PresentSupportCallback = Function<bool(const Instance *, VkPhysicalDevice device, uint32_t familyIdx)>;

	Instance(VkInstance, const PFN_vkGetInstanceProcAddr getInstanceProcAddr, uint32_t targetVersion,
			Vector<StringView> &&optionals, TerminateCallback &&terminate, PresentSupportCallback &&);
	virtual ~Instance();

	virtual Rc<gl::Device> makeDevice(uint32_t deviceIndex = maxOf<uint32_t>()) const override;

	/* Get options for physical device list for surface
	 *
	 * surface - target surface
	 * devs - device list, pair of
	 *  - VkPhysicalDevice
	 *  - uint32_t - bitmask for queue families (QF), bit is set when QF supports presentation
	 *  */
	Vector<DeviceInfo> getDeviceInfo(VkSurfaceKHR surface, const Vector<Pair<VkPhysicalDevice, uint32_t>> &devs) const;

	gl::SurfaceInfo getSurfaceOptions(VkSurfaceKHR, VkPhysicalDevice) const;
	VkExtent2D getSurfaceExtent(VkSurfaceKHR, VkPhysicalDevice) const;

	//Vector<DeviceOptions> getDevices(VkSurfaceKHR, const VkPhysicalDeviceProperties *ptr = nullptr,
	//		const Vector<Pair<VkPhysicalDevice, uint32_t>> & = Vector<Pair<VkPhysicalDevice, uint32_t>>()) const;

	VkInstance getInstance() const;

	void printDevicesInfo(std::ostream &stream) const;

	uint32_t getVersion() const { return _version; }

private:
	void getDeviceFeatures(const VkPhysicalDevice &device, DeviceInfo::Features &, ExtensionFlags, uint32_t) const;
	void getDeviceProperties(const VkPhysicalDevice &device, DeviceInfo::Properties &, ExtensionFlags, uint32_t) const;

	bool isDeviceSupportsPresent(VkPhysicalDevice device, uint32_t familyIdx) const;
	DeviceInfo getDeviceInfo(VkPhysicalDevice device) const;

	friend class VirtualDevice;
	friend class DrawDevice;
	friend class PresentationDevice;
	friend class TransferDevice;
	friend class Allocator;
	friend class ViewImpl;

#if VK_DEBUG_LOG
	VkDebugUtilsMessengerEXT debugMessenger;
#endif

	VkInstance _instance;
	uint32_t _version = 0;
	Vector<StringView> _optionals;
	Vector<DeviceInfo> _devices;
	PresentSupportCallback _checkPresentSupport;
};

}

#endif // COMPONENTS_XENOLITH_GL_XLVKINSTANCE_H_
