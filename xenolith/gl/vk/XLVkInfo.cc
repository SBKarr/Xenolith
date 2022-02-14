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

#include "XLVkInfo.h"
#include "XLVkInstance.h"

namespace stappler::xenolith::vk {

DeviceInfo::Features DeviceInfo::Features::getRequired() {
	Features ret;
	ret.device10.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
	return ret;
}

DeviceInfo::Features DeviceInfo::Features::getOptional() {
	Features ret;
	ret.device10.features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
	ret.device10.features.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
	ret.device10.features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
	ret.device10.features.multiDrawIndirect = VK_TRUE;
	ret.device10.features.shaderFloat64 = VK_TRUE;
	ret.device10.features.shaderInt64 = VK_TRUE;
	ret.device10.features.shaderInt16 = VK_TRUE;
	ret.deviceShaderFloat16Int8.shaderFloat16 = VK_TRUE;
	ret.deviceShaderFloat16Int8.shaderInt8 = VK_TRUE;
	ret.device16bitStorage.storageBuffer16BitAccess = VK_TRUE;
	//ret.device16bitStorage.uniformAndStorageBuffer16BitAccess = VK_TRUE;
	//ret.device16bitStorage.storageInputOutput16 = VK_TRUE;
	ret.device8bitStorage.storageBuffer8BitAccess = VK_TRUE;
	//ret.device8bitStorage.uniformAndStorageBuffer8BitAccess = VK_TRUE;
	ret.deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
	ret.deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	ret.deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	ret.deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingPartiallyBound = VK_TRUE;
	ret.deviceDescriptorIndexing.descriptorBindingVariableDescriptorCount = VK_TRUE;
	ret.deviceDescriptorIndexing.runtimeDescriptorArray = VK_TRUE;

	ret.flags = ExtensionFlags::Maintenance3
		| ExtensionFlags::DescriptorIndexing
		| ExtensionFlags::DrawIndirectCount
		| ExtensionFlags::Storage16Bit
		| ExtensionFlags::Storage8Bit
		| ExtensionFlags::DeviceAddress
		| ExtensionFlags::ShaderFloat16
		| ExtensionFlags::ShaderInt8
		| ExtensionFlags::MemoryBudget;

	ret.updateTo12();
	return ret;
}

bool DeviceInfo::Features::canEnable(const Features &features, uint32_t version) const {
	auto doCheck = [] (SpanView<VkBool32> src, SpanView<VkBool32> trg) {
		for (size_t i = 0; i < src.size(); ++ i) {
			if (trg[i] && !src[i]) {
				return false;
			}
		}
		return true;
	};

	if (!doCheck(
			SpanView<VkBool32>(&device10.features.robustBufferAccess, sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)),
			SpanView<VkBool32>(&features.device10.features.robustBufferAccess, sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)))) {
		return false;
	}

#define SP_VK_BOOL_ARRAY(source, field, type) \
		SpanView<VkBool32>(&source.field, (sizeof(type) - offsetof(type, field)) / sizeof(VkBool32))

#if VK_VERSION_1_2
	if (version >= VK_API_VERSION_1_2) {
		if (!doCheck(
				SP_VK_BOOL_ARRAY(device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features),
				SP_VK_BOOL_ARRAY(features.device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features))) {
			return false;
		}

		if (!doCheck(
				SP_VK_BOOL_ARRAY(device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features),
				SP_VK_BOOL_ARRAY(features.device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features))) {
			return false;
		}
	}
#endif

	if (!doCheck(
			SP_VK_BOOL_ARRAY(device16bitStorage, storageBuffer16BitAccess, VkPhysicalDevice16BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device16bitStorage, storageBuffer16BitAccess, VkPhysicalDevice16BitStorageFeaturesKHR))) {
		return false;
	}

	if (!doCheck(
			SP_VK_BOOL_ARRAY(device8bitStorage, storageBuffer8BitAccess, VkPhysicalDevice8BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device8bitStorage, storageBuffer8BitAccess, VkPhysicalDevice8BitStorageFeaturesKHR))) {
		return false;
	}

	if (!doCheck(
			SP_VK_BOOL_ARRAY(deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing, VkPhysicalDeviceDescriptorIndexingFeaturesEXT),
			SP_VK_BOOL_ARRAY(features.deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing, VkPhysicalDeviceDescriptorIndexingFeaturesEXT))) {
		return false;
	}

	if (!doCheck(
			SP_VK_BOOL_ARRAY(deviceBufferDeviceAddress, bufferDeviceAddress, VkPhysicalDeviceBufferDeviceAddressFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceBufferDeviceAddress, bufferDeviceAddress, VkPhysicalDeviceBufferDeviceAddressFeaturesKHR))) {
		return false;
	}
	if (!doCheck(
			SP_VK_BOOL_ARRAY(deviceShaderFloat16Int8, shaderFloat16, VkPhysicalDeviceShaderFloat16Int8FeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceShaderFloat16Int8, shaderFloat16, VkPhysicalDeviceShaderFloat16Int8FeaturesKHR))) {
		return false;
	}
#undef SP_VK_BOOL_ARRAY

	return true;
}

void DeviceInfo::Features::enableFromFeatures(const Features &features) {
	auto doCheck = [] (SpanView<VkBool32> src, SpanView<VkBool32> trg) {
		for (size_t i = 0; i < src.size(); ++ i) {
			if (trg[i]) {
				const_cast<VkBool32 &>(src[i]) = trg[i];
			}
		}
	};

	doCheck(
			SpanView<VkBool32>(&device10.features.robustBufferAccess, sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)),
			SpanView<VkBool32>(&features.device10.features.robustBufferAccess, sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)));

#define SP_VK_BOOL_ARRAY(source, field, type) \
		SpanView<VkBool32>(&source.field, (sizeof(type) - offsetof(type, field)) / sizeof(VkBool32))

#if VK_VERSION_1_2
	doCheck(
			SP_VK_BOOL_ARRAY(device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features),
			SP_VK_BOOL_ARRAY(features.device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features));

	doCheck(
			SP_VK_BOOL_ARRAY(device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features),
			SP_VK_BOOL_ARRAY(features.device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features));
#endif

	doCheck(
			SP_VK_BOOL_ARRAY(device16bitStorage, storageBuffer16BitAccess, VkPhysicalDevice16BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device16bitStorage, storageBuffer16BitAccess, VkPhysicalDevice16BitStorageFeaturesKHR));

	doCheck(
			SP_VK_BOOL_ARRAY(device8bitStorage, storageBuffer8BitAccess, VkPhysicalDevice8BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device8bitStorage, storageBuffer8BitAccess, VkPhysicalDevice8BitStorageFeaturesKHR));

	doCheck(
			SP_VK_BOOL_ARRAY(deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing, VkPhysicalDeviceDescriptorIndexingFeaturesEXT),
			SP_VK_BOOL_ARRAY(features.deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing, VkPhysicalDeviceDescriptorIndexingFeaturesEXT));

	doCheck(
			SP_VK_BOOL_ARRAY(deviceBufferDeviceAddress, bufferDeviceAddress, VkPhysicalDeviceBufferDeviceAddressFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceBufferDeviceAddress, bufferDeviceAddress, VkPhysicalDeviceBufferDeviceAddressFeaturesKHR));

	doCheck(
			SP_VK_BOOL_ARRAY(deviceShaderFloat16Int8, shaderFloat16, VkPhysicalDeviceShaderFloat16Int8FeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceShaderFloat16Int8, shaderFloat16, VkPhysicalDeviceShaderFloat16Int8FeaturesKHR));
#undef SP_VK_BOOL_ARRAY
}

void DeviceInfo::Features::disableFromFeatures(const Features &features) {
	auto doCheck = [] (SpanView<VkBool32> src, SpanView<VkBool32> trg) {
		for (size_t i = 0; i < src.size(); ++ i) {
			if (!trg[i]) {
				const_cast<VkBool32 &>(src[i]) = trg[i];
			}
		}
	};

	doCheck(
			SpanView<VkBool32>(&device10.features.robustBufferAccess, sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)),
			SpanView<VkBool32>(&features.device10.features.robustBufferAccess, sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32)));

#define SP_VK_BOOL_ARRAY(source, field, type) \
		SpanView<VkBool32>(&source.field, (sizeof(type) - offsetof(type, field)) / sizeof(VkBool32))

#if VK_VERSION_1_2
	doCheck(
			SP_VK_BOOL_ARRAY(device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features),
			SP_VK_BOOL_ARRAY(features.device11, storageBuffer16BitAccess, VkPhysicalDeviceVulkan11Features));

	doCheck(
			SP_VK_BOOL_ARRAY(device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features),
			SP_VK_BOOL_ARRAY(features.device12, samplerMirrorClampToEdge, VkPhysicalDeviceVulkan12Features));
#endif

	doCheck(
			SP_VK_BOOL_ARRAY(device16bitStorage, storageBuffer16BitAccess, VkPhysicalDevice16BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device16bitStorage, storageBuffer16BitAccess, VkPhysicalDevice16BitStorageFeaturesKHR));

	doCheck(
			SP_VK_BOOL_ARRAY(device8bitStorage, storageBuffer8BitAccess, VkPhysicalDevice8BitStorageFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.device8bitStorage, storageBuffer8BitAccess, VkPhysicalDevice8BitStorageFeaturesKHR));

	doCheck(
			SP_VK_BOOL_ARRAY(deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing, VkPhysicalDeviceDescriptorIndexingFeaturesEXT),
			SP_VK_BOOL_ARRAY(features.deviceDescriptorIndexing, shaderInputAttachmentArrayDynamicIndexing, VkPhysicalDeviceDescriptorIndexingFeaturesEXT));

	doCheck(
			SP_VK_BOOL_ARRAY(deviceBufferDeviceAddress, bufferDeviceAddress, VkPhysicalDeviceBufferDeviceAddressFeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceBufferDeviceAddress, bufferDeviceAddress, VkPhysicalDeviceBufferDeviceAddressFeaturesKHR));

	doCheck(
			SP_VK_BOOL_ARRAY(deviceShaderFloat16Int8, shaderFloat16, VkPhysicalDeviceShaderFloat16Int8FeaturesKHR),
			SP_VK_BOOL_ARRAY(features.deviceShaderFloat16Int8, shaderFloat16, VkPhysicalDeviceShaderFloat16Int8FeaturesKHR));
#undef SP_VK_BOOL_ARRAY
}

void DeviceInfo::Features::updateFrom12() {
#if VK_VERSION_1_2
	if (device11.storageBuffer16BitAccess == VK_TRUE) {
		flags |= ExtensionFlags::Storage16Bit;
	} else {
		flags &= (~ExtensionFlags::Storage16Bit);
	}

	device16bitStorage.storageBuffer16BitAccess = device11.storageBuffer16BitAccess;
	device16bitStorage.uniformAndStorageBuffer16BitAccess = device11.uniformAndStorageBuffer16BitAccess;
	device16bitStorage.storagePushConstant16 = device11.storagePushConstant16;
	device16bitStorage.storageInputOutput16 = device11.storageInputOutput16;

	if (device12.drawIndirectCount == VK_TRUE) {
		flags |= ExtensionFlags::DrawIndirectCount;
	} else {
		flags &= (~ExtensionFlags::DrawIndirectCount);
	}

	if (device12.storageBuffer8BitAccess == VK_TRUE) {
		flags |= ExtensionFlags::Storage8Bit;
	} else {
		flags &= (~ExtensionFlags::Storage8Bit);
	}

	device8bitStorage.storageBuffer8BitAccess = device12.storageBuffer8BitAccess;
	device8bitStorage.uniformAndStorageBuffer8BitAccess = device12.uniformAndStorageBuffer8BitAccess;
	device8bitStorage.storagePushConstant8 = device12.storagePushConstant8;

	deviceShaderFloat16Int8.shaderFloat16 = device12.shaderFloat16;
	deviceShaderFloat16Int8.shaderInt8 = device12.shaderInt8;

	if (device12.shaderFloat16 == VK_TRUE) {
		flags |= ExtensionFlags::ShaderFloat16;
	} else {
		flags &= (~ExtensionFlags::ShaderFloat16);
	}

	if (device12.shaderInt8 == VK_TRUE) {
		flags |= ExtensionFlags::ShaderInt8;
	} else {
		flags &= (~ExtensionFlags::ShaderInt8);
	}

	if (device12.descriptorIndexing == VK_TRUE) {
		flags |= ExtensionFlags::DescriptorIndexing;
	} else {
		flags &= (~ExtensionFlags::DescriptorIndexing);
	}

	deviceDescriptorIndexing.shaderInputAttachmentArrayDynamicIndexing = device12.shaderInputAttachmentArrayDynamicIndexing;
	deviceDescriptorIndexing.shaderUniformTexelBufferArrayDynamicIndexing = device12.shaderUniformTexelBufferArrayDynamicIndexing;
	deviceDescriptorIndexing.shaderStorageTexelBufferArrayDynamicIndexing = device12.shaderStorageTexelBufferArrayDynamicIndexing;
	deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexing = device12.shaderUniformBufferArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing = device12.shaderSampledImageArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexing = device12.shaderStorageBufferArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexing = device12.shaderStorageImageArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderInputAttachmentArrayNonUniformIndexing = device12.shaderInputAttachmentArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderUniformTexelBufferArrayNonUniformIndexing = device12.shaderUniformTexelBufferArrayNonUniformIndexing;
	deviceDescriptorIndexing.shaderStorageTexelBufferArrayNonUniformIndexing = device12.shaderStorageTexelBufferArrayNonUniformIndexing;
	deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind = device12.descriptorBindingUniformBufferUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind = device12.descriptorBindingSampledImageUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind = device12.descriptorBindingStorageImageUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind = device12.descriptorBindingStorageBufferUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingUniformTexelBufferUpdateAfterBind = device12.descriptorBindingUniformTexelBufferUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingStorageTexelBufferUpdateAfterBind = device12.descriptorBindingStorageTexelBufferUpdateAfterBind;
	deviceDescriptorIndexing.descriptorBindingUpdateUnusedWhilePending = device12.descriptorBindingUpdateUnusedWhilePending;
	deviceDescriptorIndexing.descriptorBindingPartiallyBound = device12.descriptorBindingPartiallyBound;
	deviceDescriptorIndexing.descriptorBindingVariableDescriptorCount = device12.descriptorBindingVariableDescriptorCount;
	deviceDescriptorIndexing.runtimeDescriptorArray = device12.runtimeDescriptorArray;

	if (device12.bufferDeviceAddress == VK_TRUE) {
		flags |= ExtensionFlags::DeviceAddress;
	} else {
		flags &= (~ExtensionFlags::DeviceAddress);
	}

	deviceBufferDeviceAddress.bufferDeviceAddress = device12.bufferDeviceAddress;
	deviceBufferDeviceAddress.bufferDeviceAddressCaptureReplay = device12.bufferDeviceAddressCaptureReplay;
	deviceBufferDeviceAddress.bufferDeviceAddressMultiDevice = device12.bufferDeviceAddressMultiDevice;
#endif
}

void DeviceInfo::Features::updateTo12(bool updateFlags) {
	if (updateFlags) {
		if ((flags & ExtensionFlags::Storage16Bit) != ExtensionFlags::None) {
			if (device16bitStorage.storageBuffer16BitAccess == VK_TRUE) {
				flags |= ExtensionFlags::Storage16Bit;
			} else {
				flags &= (~ExtensionFlags::Storage16Bit);
			}
		}
		if ((flags & ExtensionFlags::Storage8Bit) != ExtensionFlags::None) {
			if (device8bitStorage.storageBuffer8BitAccess == VK_TRUE) {
				flags |= ExtensionFlags::Storage8Bit;
			} else {
				flags &= (~ExtensionFlags::Storage8Bit);
			}
		}

		if ((flags & ExtensionFlags::ShaderFloat16) != ExtensionFlags::None || (flags & ExtensionFlags::ShaderInt8) != ExtensionFlags::None) {
			if (deviceShaderFloat16Int8.shaderInt8 == VK_TRUE) {
				flags |= ExtensionFlags::ShaderInt8;
			} else {
				flags &= (~ExtensionFlags::ShaderInt8);
			}
			if (deviceShaderFloat16Int8.shaderFloat16 == VK_TRUE) {
				flags |= ExtensionFlags::ShaderFloat16;
			} else {
				flags &= (~ExtensionFlags::ShaderFloat16);
			}
		}

		if ((flags & ExtensionFlags::DeviceAddress) != ExtensionFlags::None) {
			if (deviceBufferDeviceAddress.bufferDeviceAddress == VK_TRUE) {
				flags |= ExtensionFlags::DeviceAddress;
			} else {
				flags &= (~ExtensionFlags::DeviceAddress);
			}
		}
	}

#if VK_VERSION_1_2
	device11.storageBuffer16BitAccess = device16bitStorage.storageBuffer16BitAccess;
	device11.uniformAndStorageBuffer16BitAccess = device16bitStorage.storageBuffer16BitAccess;
	device11.storagePushConstant16 = device16bitStorage.storageBuffer16BitAccess;
	device11.storageInputOutput16 = device16bitStorage.storageBuffer16BitAccess;

	if ((flags & ExtensionFlags::DrawIndirectCount) != ExtensionFlags::None) {
		device12.drawIndirectCount = VK_TRUE;
	}

	device12.storageBuffer8BitAccess = device8bitStorage.storageBuffer8BitAccess;
	device12.uniformAndStorageBuffer8BitAccess = device8bitStorage.uniformAndStorageBuffer8BitAccess;
	device12.storagePushConstant8 = device8bitStorage.storagePushConstant8;

	device12.shaderFloat16 = deviceShaderFloat16Int8.shaderFloat16;
	device12.shaderInt8 = deviceShaderFloat16Int8.shaderInt8;

	if ((flags & ExtensionFlags::DescriptorIndexing) != ExtensionFlags::None) {
		device12.descriptorIndexing = VK_TRUE;
	}

	device12.shaderInputAttachmentArrayDynamicIndexing = deviceDescriptorIndexing.shaderInputAttachmentArrayDynamicIndexing;
	device12.shaderUniformTexelBufferArrayDynamicIndexing = deviceDescriptorIndexing.shaderUniformTexelBufferArrayDynamicIndexing;
	device12.shaderStorageTexelBufferArrayDynamicIndexing = deviceDescriptorIndexing.shaderStorageTexelBufferArrayDynamicIndexing;
	device12.shaderUniformBufferArrayNonUniformIndexing = deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexing;
	device12.shaderSampledImageArrayNonUniformIndexing = deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing;
	device12.shaderStorageBufferArrayNonUniformIndexing = deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexing;
	device12.shaderStorageImageArrayNonUniformIndexing = deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexing;
	device12.shaderInputAttachmentArrayNonUniformIndexing = deviceDescriptorIndexing.shaderInputAttachmentArrayNonUniformIndexing;
	device12.shaderUniformTexelBufferArrayNonUniformIndexing = deviceDescriptorIndexing.shaderUniformTexelBufferArrayNonUniformIndexing;
	device12.shaderStorageTexelBufferArrayNonUniformIndexing = deviceDescriptorIndexing.shaderStorageTexelBufferArrayNonUniformIndexing;
	device12.descriptorBindingUniformBufferUpdateAfterBind = deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind;
	device12.descriptorBindingSampledImageUpdateAfterBind = deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind;
	device12.descriptorBindingStorageImageUpdateAfterBind = deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind;
	device12.descriptorBindingStorageBufferUpdateAfterBind = deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind;
	device12.descriptorBindingUniformTexelBufferUpdateAfterBind = deviceDescriptorIndexing.descriptorBindingUniformTexelBufferUpdateAfterBind;
	device12.descriptorBindingStorageTexelBufferUpdateAfterBind = deviceDescriptorIndexing.descriptorBindingStorageTexelBufferUpdateAfterBind;
	device12.descriptorBindingUpdateUnusedWhilePending = deviceDescriptorIndexing.descriptorBindingUpdateUnusedWhilePending;
	device12.descriptorBindingPartiallyBound = deviceDescriptorIndexing.descriptorBindingPartiallyBound;
	device12.descriptorBindingVariableDescriptorCount = deviceDescriptorIndexing.descriptorBindingVariableDescriptorCount;
	device12.runtimeDescriptorArray = deviceDescriptorIndexing.runtimeDescriptorArray;

	device12.bufferDeviceAddress = deviceBufferDeviceAddress.bufferDeviceAddress;
	device12.bufferDeviceAddressCaptureReplay = deviceBufferDeviceAddress.bufferDeviceAddressCaptureReplay;
	device12.bufferDeviceAddressMultiDevice = deviceBufferDeviceAddress.bufferDeviceAddressMultiDevice;
#endif
}

DeviceInfo::Features::Features() { }

DeviceInfo::Features::Features(const Features &f) {
	memcpy(this, &f, sizeof(Features));
}

DeviceInfo::Features &DeviceInfo::Features::operator=(const Features &f) {
	memcpy(this, &f, sizeof(Features));
	return *this;
}

DeviceInfo::Properties::Properties() { }

DeviceInfo::Properties::Properties(const Properties &p) {
	memcpy(this, &p, sizeof(Properties)); }

DeviceInfo::Properties &DeviceInfo::Properties::operator=(const Properties &p) {
	memcpy(this, &p, sizeof(Properties));
	return *this;
}

DeviceInfo::DeviceInfo() { }

DeviceInfo::DeviceInfo(VkPhysicalDevice dev, QueueFamilyInfo gr, QueueFamilyInfo pres, QueueFamilyInfo tr, QueueFamilyInfo comp,
		Vector<StringView> &&optionals, Vector<StringView> &&promoted)
: device(dev), graphicsFamily(gr), presentFamily(pres), transferFamily(tr), computeFamily(comp)
, optionalExtensions(move(optionals)), promotedExtensions(move(promoted)) { }

bool DeviceInfo::isUsable() const {
	if ((graphicsFamily.ops & QueueOperations::Graphics) != QueueOperations::None
			&& (presentFamily.ops & QueueOperations::Present) != QueueOperations::None
			&& (transferFamily.ops & QueueOperations::Transfer) != QueueOperations::None
			&& (computeFamily.ops & QueueOperations::Compute) != QueueOperations::None
			&& requiredFeaturesExists && requiredExtensionsExists) {
		return true;
	}
	return false;
}

String DeviceInfo::description() const {
	StringStream stream;
	stream << "\t\t[Queue] ";

	if ((graphicsFamily.ops & QueueOperations::Graphics) != QueueOperations::None) {
		stream << "Graphics: [" << graphicsFamily.index << "]; ";
	} else {
		stream << "Graphics: [Not available]; ";
	}

	if ((presentFamily.ops & QueueOperations::Present) != QueueOperations::None) {
		stream << "Presentation: [" << presentFamily.index << "]; ";
	} else {
		stream << "Presentation: [Not available]; ";
	}

	if ((transferFamily.ops & QueueOperations::Transfer) != QueueOperations::None) {
		stream << "Transfer: [" << transferFamily.index << "]; ";
	} else {
		stream << "Transfer: [Not available]; ";
	}

	if ((computeFamily.ops & QueueOperations::Compute) != QueueOperations::None) {
		stream << "Compute: [" << computeFamily.index << "];\n";
	} else {
		stream << "Compute: [Not available];\n";
	}

	stream << "\t\t[Limits: Samplers]"
			" PerSet: " << properties.device10.properties.limits.maxDescriptorSetSamplers
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindSamplers << ");"
			" PerStage: " << properties.device10.properties.limits.maxPerStageDescriptorSamplers
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindSamplers << ");"
			"\n";

	stream << "\t\t[Limits: UniformBuffers]"
			" PerSet: " << properties.device10.properties.limits.maxDescriptorSetUniformBuffers
			<< " dyn: " << properties.device10.properties.limits.maxDescriptorSetUniformBuffersDynamic
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindUniformBuffers
			<< " dyn: " << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic << ");"
			" PerStage: " << properties.device10.properties.limits.maxPerStageDescriptorUniformBuffers
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindUniformBuffers << ");"
			<< (properties.deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexingNative ? StringView(" NonUniformIndexingNative;") : StringView())
			<< "\n";

	stream << "\t\t[Limits: StorageBuffers]"
			" PerSet: " << properties.device10.properties.limits.maxDescriptorSetStorageBuffers
			<< " dyn: " << properties.device10.properties.limits.maxDescriptorSetStorageBuffersDynamic
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindStorageBuffers
			<< " dyn: " << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic << ");"
			" PerStage: " << properties.device10.properties.limits.maxPerStageDescriptorStorageBuffers
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindStorageBuffers << ");"
			<< (properties.deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexingNative ? StringView(" NonUniformIndexingNative;") : StringView())
			<< "\n";

	stream << "\t\t[Limits: SampledImages]"
			" PerSet: " << properties.device10.properties.limits.maxDescriptorSetSampledImages
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindSampledImages << ");"
			" PerStage: " << properties.device10.properties.limits.maxPerStageDescriptorSampledImages
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindSampledImages << ");"
			<< (properties.deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexingNative ? StringView(" NonUniformIndexingNative;") : StringView())
			<< "\n";

	stream << "\t\t[Limits: StorageImages]"
			" PerSet: " << properties.device10.properties.limits.maxDescriptorSetStorageImages
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindStorageImages << ");"
			" PerStage: " << properties.device10.properties.limits.maxPerStageDescriptorStorageImages
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindStorageImages << ");"
			<< (properties.deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexingNative ? StringView(" NonUniformIndexingNative;") : StringView())
			<< "\n";

	stream << "\t\t[Limits: InputAttachments]"
			" PerSet: " << properties.device10.properties.limits.maxDescriptorSetInputAttachments
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxDescriptorSetUpdateAfterBindInputAttachments << ");"
			" PerStage: " << properties.device10.properties.limits.maxPerStageDescriptorInputAttachments
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxPerStageDescriptorUpdateAfterBindInputAttachments << ");"
			<< (properties.deviceDescriptorIndexing.shaderInputAttachmentArrayNonUniformIndexingNative ? StringView(" NonUniformIndexingNative;") : StringView())
			<< "\n";

	stream << "\t\t[Limits: Resources]"
			" PerStage: " << properties.device10.properties.limits.maxPerStageDescriptorInputAttachments
			<< " (updatable: " << properties.deviceDescriptorIndexing.maxPerStageUpdateAfterBindResources << ");"
			"\n";
	stream << "\t\t[Limits: Allocations] " << properties.device10.properties.limits.maxMemoryAllocationCount << " blocks, "
			<< properties.device10.properties.limits.maxSamplerAllocationCount << " samplers;\n";
	stream << "\t\t[Limits: Ranges] Uniform: " << properties.device10.properties.limits.maxUniformBufferRange << ", Storage: "
			<< properties.device10.properties.limits.maxStorageBufferRange << ";\n";
	stream << "\t\t[Limits: DrawIndirectCount] " << properties.device10.properties.limits.maxDrawIndirectCount << ";\n";

	/*uint32_t extensionCount;
	instance->vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	Vector<VkExtensionProperties> availableExtensions(extensionCount);
	instance->vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	stream << "\t[Extensions]\n";
	for (auto &it : availableExtensions) {
		stream << "\t\t" << it.extensionName << ": " << vk::Instance::getVersionDescription(it.specVersion) << "\n";
	}*/

	return stream.str();
}

}
