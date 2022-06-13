/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef COMPONENTS_XENOLITH_GL_XLVK_H_
#define COMPONENTS_XENOLITH_GL_XLVK_H_

#include "XLDefine.h"
#include "XLGl.h"

#if LINUX
#define VK_USE_PLATFORM_XCB_KHR 1
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#endif

#include <vulkan/vulkan.h>

#if LINUX
#define VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR 0x00000400
#define VK_BUFFER_USAGE_RAY_TRACING_BIT_NV VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR
#endif

namespace stappler::xenolith::vk {

#if DEBUG
#define VK_DEBUG_LOG 1
#define VK_HOOK_DEBUG 1 // enable engine hooks for Vulkan calls
static constexpr bool s_enableValidationLayers = true;

#else
#define VK_DEBUG_LOG 0
#define VK_HOOK_DEBUG 0 // enable engine hooks for Vulkan calls
static constexpr bool s_enableValidationLayers = false;
#endif

[[maybe_unused]] static const char * const s_validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};

}

#include "XLVkTable.h"

namespace stappler::xenolith::vk {

class Instance;

static const char * const s_requiredExtension[] = {
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	nullptr
};

static const char * const s_optionalExtension[] = {
	nullptr
};

static const char * const s_requiredDeviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
	nullptr
};

static const char * const s_optionalDeviceExtensions[] = {
	// Descriptor indexing
	VK_KHR_MAINTENANCE3_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,

	// DrawInderectCount
	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,

	// 16-bit, 8-bit shader storage
	VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,

	// BufferDeviceAddress
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	nullptr
};

enum class ExtensionFlags {
	None,
	Maintenance3 = 1 << 0,
	DescriptorIndexing = 1 << 1,
	DrawIndirectCount = 1 << 2,
	Storage16Bit =  1 << 3,
	Storage8Bit = 1 << 4,
	DeviceAddress = 1 << 5,
	ShaderFloat16 = 1 << 6,
	ShaderInt8 = 1 << 7,
	MemoryBudget = 1 << 8,
	GetMemoryRequirements2 = 1 << 9,
	DedicatedAllocation = 1 << 10,
};

SP_DEFINE_ENUM_AS_MASK(ExtensionFlags);

static const char * const s_promotedVk11Extensions[] = {
	VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
	VK_KHR_DEVICE_GROUP_EXTENSION_NAME,
	VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
	VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	VK_KHR_MAINTENANCE1_EXTENSION_NAME,
	VK_KHR_MAINTENANCE2_EXTENSION_NAME,
	VK_KHR_MAINTENANCE3_EXTENSION_NAME,
	VK_KHR_MULTIVIEW_EXTENSION_NAME,
	VK_KHR_RELAXED_BLOCK_LAYOUT_EXTENSION_NAME,
	VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
	VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
	VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME,
	VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME,
	nullptr
};

static const char * const s_promotedVk12Extensions[] = {
	VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
	VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
	VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
	VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,
	VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
	VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,
	VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
	VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME,
	VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
	VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
	VK_KHR_SHADER_SUBGROUP_EXTENDED_TYPES_EXTENSION_NAME,
	VK_KHR_SPIRV_1_4_EXTENSION_NAME,
	VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
	VK_KHR_UNIFORM_BUFFER_STANDARD_LAYOUT_EXTENSION_NAME,
	VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,
	VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
	VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
	VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME,
	VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
	VK_EXT_SEPARATE_STENCIL_USAGE_EXTENSION_NAME,
	VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
	nullptr
};

static constexpr bool s_printVkInfo = true;

enum class QueueOperations : uint32_t {
	None,
	Graphics = VK_QUEUE_GRAPHICS_BIT,
	Compute = VK_QUEUE_COMPUTE_BIT,
	Transfer = VK_QUEUE_TRANSFER_BIT,
	SparceBinding = VK_QUEUE_SPARSE_BINDING_BIT,
	Present = 0x8000'0000,
};

SP_DEFINE_ENUM_AS_MASK(QueueOperations)

enum class PresentationEvent {
	Update, // force-update
	SwapChainDeprecated, // swapchain was deprecated by view
	SwapChainRecreated, // swapchain was recreated by view
	SwapChainForceRecreate, // force engine to recreate swapchain with best params
	FrameImageAcquired, // image from swapchain successfully acquired
	FramePresentReady, // frame ready for presentation
	FrameTimeoutPassed, // framerate heartbeat
	UpdateFrameInterval, // view wants us to update frame interval
	CompileResource, // new GL resource requested
	Exit,
};

QueueOperations getQueueOperations(VkQueueFlags, bool present);
QueueOperations getQueueOperations(gl::RenderPassType);
String getQueueOperationsDesc(QueueOperations);
VkShaderStageFlagBits getVkStageBits(gl::ProgramStage);

StringView getVkFormatName(VkFormat fmt);
StringView getVkColorSpaceName(VkColorSpaceKHR fmt);

String getVkMemoryPropertyFlags(VkMemoryPropertyFlags);

bool checkIfExtensionAvailable(uint32_t apiVersion, const char *name, const Vector<VkExtensionProperties> &available,
		Vector<StringView> &optionals, Vector<StringView> &promoted, ExtensionFlags &flags);

bool isPromotedExtension(uint32_t apiVersion, StringView name);

size_t getFormatBlockSize(VkFormat);

VkPresentModeKHR getVkPresentMode(gl::PresentMode presentMode);

template <typename T>
void sanitizeVkStruct(T &t) {
	::memset(&t, 0, sizeof(T));
}

}

std::ostream &operator<< (std::ostream &stream, VkResult res);

#endif /* COMPONENTS_XENOLITH_GL_XLVK_H_ */
