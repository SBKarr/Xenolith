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

#include "XLDefine.h"
#include "XLVk.h"

#define XL_VK_DEBUG 0
#define XL_VKAPI_DEBUG 0

#if XL_VK_DEBUG
#define XL_VK_LOG(...) log::vtext("Vk::Loop", __VA_ARGS__)
#else
#define XL_VK_LOG(...)
#endif

#if XL_VKAPI_DEBUG
#define XL_VKAPI_LOG(...) log::vtext("vk::Api", __VA_ARGS__)
#else
#define XL_VKAPI_LOG(...)
#endif

#include "XLVkLoop.cc"
#include "XLVkInstance.cc"
#include "XLVkTable.cc"
#include "XLVkInfo.cc"
#include "XLVkView.cc"

namespace stappler::xenolith::vk {

QueueOperations getQueueOperations(VkQueueFlags flags, bool present) {
	QueueOperations ret = QueueOperations(flags) &
			(QueueOperations::Graphics | QueueOperations::Compute | QueueOperations::Transfer | QueueOperations::SparceBinding);
	if (present) {
		ret |= QueueOperations::Present;
	}
	return ret;
}

QueueOperations getQueueOperations(gl::RenderPassType type) {
	switch (type) {
	case gl::RenderPassType::Graphics: return QueueOperations::Graphics; break;
	case gl::RenderPassType::Compute: return QueueOperations::Compute; break;
	case gl::RenderPassType::Transfer: return QueueOperations::Transfer; break;
	case gl::RenderPassType::Generic: return QueueOperations::None; break;
	}
	return QueueOperations::None;
}

String getQueueOperationsDesc(QueueOperations ops) {
	StringStream stream;
	if ((ops & QueueOperations::Graphics) != QueueOperations::None) { stream << " Graphics"; }
	if ((ops & QueueOperations::Compute) != QueueOperations::None) { stream << " Compute"; }
	if ((ops & QueueOperations::Transfer) != QueueOperations::None) { stream << " Transfer"; }
	if ((ops & QueueOperations::SparceBinding) != QueueOperations::None) { stream << " SparceBinding"; }
	if ((ops & QueueOperations::Present) != QueueOperations::None) { stream << " Present"; }
	return stream.str();
}

VkShaderStageFlagBits getVkStageBits(gl::ProgramStage stage) {
	return VkShaderStageFlagBits(stage);
}

StringView getVkFormatName(VkFormat fmt) {
	switch (fmt) {
	case VK_FORMAT_UNDEFINED: return "UNDEFINED"; break;
	case VK_FORMAT_R4G4_UNORM_PACK8: return "R4G4_UNORM_PACK8"; break;
	case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return "R4G4B4A4_UNORM_PACK16"; break;
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return "B4G4R4A4_UNORM_PACK16"; break;
	case VK_FORMAT_R5G6B5_UNORM_PACK16: return "R5G6B5_UNORM_PACK16"; break;
	case VK_FORMAT_B5G6R5_UNORM_PACK16: return "B5G6R5_UNORM_PACK16"; break;
	case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return "R5G5B5A1_UNORM_PACK16"; break;
	case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return "B5G5R5A1_UNORM_PACK16"; break;
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return "A1R5G5B5_UNORM_PACK16"; break;
	case VK_FORMAT_R8_UNORM: return "R8_UNORM"; break;
	case VK_FORMAT_R8_SNORM: return "R8_SNORM"; break;
	case VK_FORMAT_R8_USCALED: return "R8_USCALED"; break;
	case VK_FORMAT_R8_SSCALED: return "R8_SSCALED"; break;
	case VK_FORMAT_R8_UINT: return "R8_UINT"; break;
	case VK_FORMAT_R8_SINT: return "R8_SINT"; break;
	case VK_FORMAT_R8_SRGB: return "R8_SRGB"; break;
	case VK_FORMAT_R8G8_UNORM: return "R8G8_UNORM"; break;
	case VK_FORMAT_R8G8_SNORM: return "R8G8_SNORM"; break;
	case VK_FORMAT_R8G8_USCALED: return "R8G8_USCALED"; break;
	case VK_FORMAT_R8G8_SSCALED: return "R8G8_SSCALED"; break;
	case VK_FORMAT_R8G8_UINT: return "R8G8_UINT"; break;
	case VK_FORMAT_R8G8_SINT: return "R8G8_SINT"; break;
	case VK_FORMAT_R8G8_SRGB: return "R8G8_SRGB"; break;
	case VK_FORMAT_R8G8B8_UNORM: return "R8G8B8_UNORM"; break;
	case VK_FORMAT_R8G8B8_SNORM: return "R8G8B8_SNORM"; break;
	case VK_FORMAT_R8G8B8_USCALED: return "R8G8B8_USCALED"; break;
	case VK_FORMAT_R8G8B8_SSCALED: return "R8G8B8_SSCALED"; break;
	case VK_FORMAT_R8G8B8_UINT: return "R8G8B8_UINT"; break;
	case VK_FORMAT_R8G8B8_SINT: return "R8G8B8_SINT"; break;
	case VK_FORMAT_R8G8B8_SRGB: return "R8G8B8_SRGB"; break;
	case VK_FORMAT_B8G8R8_UNORM: return "B8G8R8_UNORM"; break;
	case VK_FORMAT_B8G8R8_SNORM: return "B8G8R8_SNORM"; break;
	case VK_FORMAT_B8G8R8_USCALED: return "B8G8R8_USCALED"; break;
	case VK_FORMAT_B8G8R8_SSCALED: return "B8G8R8_SSCALED"; break;
	case VK_FORMAT_B8G8R8_UINT: return "B8G8R8_UINT"; break;
	case VK_FORMAT_B8G8R8_SINT: return "B8G8R8_SINT"; break;
	case VK_FORMAT_B8G8R8_SRGB: return "B8G8R8_SRGB"; break;
	case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM"; break;
	case VK_FORMAT_R8G8B8A8_SNORM: return "R8G8B8A8_SNORM"; break;
	case VK_FORMAT_R8G8B8A8_USCALED: return "R8G8B8A8_USCALED"; break;
	case VK_FORMAT_R8G8B8A8_SSCALED: return "R8G8B8A8_SSCALED"; break;
	case VK_FORMAT_R8G8B8A8_UINT: return "R8G8B8A8_UINT"; break;
	case VK_FORMAT_R8G8B8A8_SINT: return "R8G8B8A8_SINT"; break;
	case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB"; break;
	case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM"; break;
	case VK_FORMAT_B8G8R8A8_SNORM: return "B8G8R8A8_SNORM"; break;
	case VK_FORMAT_B8G8R8A8_USCALED: return "B8G8R8A8_USCALED"; break;
	case VK_FORMAT_B8G8R8A8_SSCALED: return "B8G8R8A8_SSCALED"; break;
	case VK_FORMAT_B8G8R8A8_UINT: return "B8G8R8A8_UINT"; break;
	case VK_FORMAT_B8G8R8A8_SINT: return "B8G8R8A8_SINT"; break;
	case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB"; break;
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return "A8B8G8R8_UNORM_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return "A8B8G8R8_SNORM_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return "A8B8G8R8_USCALED_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return "A8B8G8R8_SSCALED_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_UINT_PACK32: return "A8B8G8R8_UINT_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_SINT_PACK32: return "A8B8G8R8_SINT_PACK32"; break;
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return "A8B8G8R8_SRGB_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "A2R10G10B10_UNORM_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return "A2R10G10B10_SNORM_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return "A2R10G10B10_USCALED_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return "A2R10G10B10_SSCALED_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_UINT_PACK32: return "A2R10G10B10_UINT_PACK32"; break;
	case VK_FORMAT_A2R10G10B10_SINT_PACK32: return "A2R10G10B10_SINT_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "A2B10G10R10_UNORM_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return "A2B10G10R10_SNORM_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return "A2B10G10R10_USCALED_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return "A2B10G10R10_SSCALED_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_UINT_PACK32: return "A2B10G10R10_UINT_PACK32"; break;
	case VK_FORMAT_A2B10G10R10_SINT_PACK32: return "A2B10G10R10_SINT_PACK32"; break;
	case VK_FORMAT_R16_UNORM: return "R16_UNORM"; break;
	case VK_FORMAT_R16_SNORM: return "R16_SNORM"; break;
	case VK_FORMAT_R16_USCALED: return "R16_USCALED"; break;
	case VK_FORMAT_R16_SSCALED: return "R16_SSCALED"; break;
	case VK_FORMAT_R16_UINT: return "R16_UINT"; break;
	case VK_FORMAT_R16_SINT: return "R16_SINT"; break;
	case VK_FORMAT_R16_SFLOAT: return "R16_SFLOAT"; break;
	case VK_FORMAT_R16G16_UNORM: return "R16G16_UNORM"; break;
	case VK_FORMAT_R16G16_SNORM: return "R16G16_SNORM"; break;
	case VK_FORMAT_R16G16_USCALED: return "R16G16_USCALED"; break;
	case VK_FORMAT_R16G16_SSCALED: return "R16G16_SSCALED"; break;
	case VK_FORMAT_R16G16_UINT: return "R16G16_UINT"; break;
	case VK_FORMAT_R16G16_SINT: return "R16G16_SINT"; break;
	case VK_FORMAT_R16G16_SFLOAT: return "R16G16_SFLOAT"; break;
	case VK_FORMAT_R16G16B16_UNORM: return "R16G16B16_UNORM"; break;
	case VK_FORMAT_R16G16B16_SNORM: return "R16G16B16_SNORM"; break;
	case VK_FORMAT_R16G16B16_USCALED: return "R16G16B16_USCALED"; break;
	case VK_FORMAT_R16G16B16_SSCALED: return "R16G16B16_SSCALED"; break;
	case VK_FORMAT_R16G16B16_UINT: return "R16G16B16_UINT"; break;
	case VK_FORMAT_R16G16B16_SINT: return "R16G16B16_SINT"; break;
	case VK_FORMAT_R16G16B16_SFLOAT: return "R16G16B16_SFLOAT"; break;
	case VK_FORMAT_R16G16B16A16_UNORM: return "R16G16B16A16_UNORM"; break;
	case VK_FORMAT_R16G16B16A16_SNORM: return "R16G16B16A16_SNORM"; break;
	case VK_FORMAT_R16G16B16A16_USCALED: return "R16G16B16A16_USCALED"; break;
	case VK_FORMAT_R16G16B16A16_SSCALED: return "R16G16B16A16_SSCALED"; break;
	case VK_FORMAT_R16G16B16A16_UINT: return "R16G16B16A16_UINT"; break;
	case VK_FORMAT_R16G16B16A16_SINT: return "R16G16B16A16_SINT"; break;
	case VK_FORMAT_R16G16B16A16_SFLOAT: return "R16G16B16A16_SFLOAT"; break;
	case VK_FORMAT_R32_UINT: return "R32_UINT"; break;
	case VK_FORMAT_R32_SINT: return "R32_SINT"; break;
	case VK_FORMAT_R32_SFLOAT: return "R32_SFLOAT"; break;
	case VK_FORMAT_R32G32_UINT: return "R32G32_UINT"; break;
	case VK_FORMAT_R32G32_SINT: return "R32G32_SINT"; break;
	case VK_FORMAT_R32G32_SFLOAT: return "R32G32_SFLOAT"; break;
	case VK_FORMAT_R32G32B32_UINT: return "R32G32B32_UINT"; break;
	case VK_FORMAT_R32G32B32_SINT: return "R32G32B32_SINT"; break;
	case VK_FORMAT_R32G32B32_SFLOAT: return "R32G32B32_SFLOAT"; break;
	case VK_FORMAT_R32G32B32A32_UINT: return "R32G32B32A32_UINT"; break;
	case VK_FORMAT_R32G32B32A32_SINT: return "R32G32B32A32_SINT"; break;
	case VK_FORMAT_R32G32B32A32_SFLOAT: return "R32G32B32A32_SFLOAT"; break;
	case VK_FORMAT_R64_UINT: return "R64_UINT"; break;
	case VK_FORMAT_R64_SINT: return "R64_SINT"; break;
	case VK_FORMAT_R64_SFLOAT: return "R64_SFLOAT"; break;
	case VK_FORMAT_R64G64_UINT: return "R64G64_UINT"; break;
	case VK_FORMAT_R64G64_SINT: return "R64G64_SINT"; break;
	case VK_FORMAT_R64G64_SFLOAT: return "R64G64_SFLOAT"; break;
	case VK_FORMAT_R64G64B64_UINT: return "R64G64B64_UINT"; break;
	case VK_FORMAT_R64G64B64_SINT: return "R64G64B64_SINT"; break;
	case VK_FORMAT_R64G64B64_SFLOAT: return "R64G64B64_SFLOAT"; break;
	case VK_FORMAT_R64G64B64A64_UINT: return "R64G64B64A64_UINT"; break;
	case VK_FORMAT_R64G64B64A64_SINT: return "R64G64B64A64_SINT"; break;
	case VK_FORMAT_R64G64B64A64_SFLOAT: return "R64G64B64A64_SFLOAT"; break;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return "B10G11R11_UFLOAT_PACK32"; break;
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return "E5B9G9R9_UFLOAT_PACK32"; break;
	case VK_FORMAT_D16_UNORM: return "D16_UNORM"; break;
	case VK_FORMAT_X8_D24_UNORM_PACK32: return "X8_D24_UNORM_PACK32"; break;
	case VK_FORMAT_D32_SFLOAT: return "D32_SFLOAT"; break;
	case VK_FORMAT_S8_UINT: return "S8_UINT"; break;
	case VK_FORMAT_D16_UNORM_S8_UINT: return "D16_UNORM_S8_UINT"; break;
	case VK_FORMAT_D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT"; break;
	case VK_FORMAT_D32_SFLOAT_S8_UINT: return "D32_SFLOAT_S8_UINT"; break;
	case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return "BC1_RGB_UNORM_BLOCK"; break;
	case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return "BC1_RGB_SRGB_BLOCK"; break;
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return "BC1_RGBA_UNORM_BLOCK"; break;
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return "BC1_RGBA_SRGB_BLOCK"; break;
	case VK_FORMAT_BC2_UNORM_BLOCK: return "BC2_UNORM_BLOCK"; break;
	case VK_FORMAT_BC2_SRGB_BLOCK: return "BC2_SRGB_BLOCK"; break;
	case VK_FORMAT_BC3_UNORM_BLOCK: return "BC3_UNORM_BLOCK"; break;
	case VK_FORMAT_BC3_SRGB_BLOCK: return "BC3_SRGB_BLOCK"; break;
	case VK_FORMAT_BC4_UNORM_BLOCK: return "BC4_UNORM_BLOCK"; break;
	case VK_FORMAT_BC4_SNORM_BLOCK: return "BC4_SNORM_BLOCK"; break;
	case VK_FORMAT_BC5_UNORM_BLOCK: return "BC5_UNORM_BLOCK"; break;
	case VK_FORMAT_BC5_SNORM_BLOCK: return "BC5_SNORM_BLOCK"; break;
	case VK_FORMAT_BC6H_UFLOAT_BLOCK: return "BC6H_UFLOAT_BLOCK"; break;
	case VK_FORMAT_BC6H_SFLOAT_BLOCK: return "BC6H_SFLOAT_BLOCK"; break;
	case VK_FORMAT_BC7_UNORM_BLOCK: return "BC7_UNORM_BLOCK"; break;
	case VK_FORMAT_BC7_SRGB_BLOCK: return "BC7_SRGB_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return "ETC2_R8G8B8_UNORM_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return "ETC2_R8G8B8_SRGB_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return "ETC2_R8G8B8A1_UNORM_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return "ETC2_R8G8B8A1_SRGB_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return "ETC2_R8G8B8A8_UNORM_BLOCK"; break;
	case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return "ETC2_R8G8B8A8_SRGB_BLOCK"; break;
	case VK_FORMAT_EAC_R11_UNORM_BLOCK: return "EAC_R11_UNORM_BLOCK"; break;
	case VK_FORMAT_EAC_R11_SNORM_BLOCK: return "EAC_R11_SNORM_BLOCK"; break;
	case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return "EAC_R11G11_UNORM_BLOCK"; break;
	case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return "EAC_R11G11_SNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return "ASTC_4x4_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return "ASTC_4x4_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return "ASTC_5x4_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return "ASTC_5x4_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return "ASTC_5x5_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return "ASTC_5x5_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return "ASTC_6x5_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return "ASTC_6x5_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return "ASTC_6x6_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return "ASTC_6x6_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return "ASTC_8x5_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return "ASTC_8x5_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return "ASTC_8x6_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return "ASTC_8x6_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return "ASTC_8x8_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return "ASTC_8x8_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return "ASTC_10x5_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return "ASTC_10x5_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return "ASTC_10x6_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return "ASTC_10x6_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return "ASTC_10x8_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return "ASTC_10x8_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return "ASTC_10x10_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return "ASTC_10x10_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return "ASTC_12x10_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return "ASTC_12x10_SRGB_BLOCK"; break;
	case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return "ASTC_12x12_UNORM_BLOCK"; break;
	case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return "ASTC_12x12_SRGB_BLOCK"; break;
	case VK_FORMAT_G8B8G8R8_422_UNORM: return "G8B8G8R8_422_UNORM"; break;
	case VK_FORMAT_B8G8R8G8_422_UNORM: return "B8G8R8G8_422_UNORM"; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return "G8_B8_R8_3PLANE_420_UNORM"; break;
	case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return "G8_B8R8_2PLANE_420_UNORM"; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return "G8_B8_R8_3PLANE_422_UNORM"; break;
	case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return "G8_B8R8_2PLANE_422_UNORM"; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return "G8_B8_R8_3PLANE_444_UNORM"; break;
	case VK_FORMAT_R10X6_UNORM_PACK16: return "R10X6_UNORM_PACK16"; break;
	case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return "R10X6G10X6_UNORM_2PACK16"; break;
	case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return "R10X6G10X6B10X6A10X6_UNORM_4PACK16"; break;
	case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return "G10X6B10X6G10X6R10X6_422_UNORM_4PACK16"; break;
	case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return "B10X6G10X6R10X6G10X6_422_UNORM_4PACK16"; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return "G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16"; break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return "G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16"; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return "G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16"; break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return "G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16"; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return "G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16"; break;
	case VK_FORMAT_R12X4_UNORM_PACK16: return "R12X4_UNORM_PACK16"; break;
	case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return "R12X4G12X4_UNORM_2PACK16"; break;
	case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return "R12X4G12X4B12X4A12X4_UNORM_4PACK16"; break;
	case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return "G12X4B12X4G12X4R12X4_422_UNORM_4PACK16"; break;
	case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return "B12X4G12X4R12X4G12X4_422_UNORM_4PACK16"; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return "G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16"; break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return "G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16"; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return "G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16"; break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return "G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16"; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return "G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16"; break;
	case VK_FORMAT_G16B16G16R16_422_UNORM: return "G16B16G16R16_422_UNORM"; break;
	case VK_FORMAT_B16G16R16G16_422_UNORM: return "B16G16R16G16_422_UNORM"; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return "G16_B16_R16_3PLANE_420_UNORM"; break;
	case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return "G16_B16R16_2PLANE_420_UNORM"; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return "G16_B16_R16_3PLANE_422_UNORM"; break;
	case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return "G16_B16R16_2PLANE_422_UNORM"; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return "G16_B16_R16_3PLANE_444_UNORM"; break;
	case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return "PVRTC1_2BPP_UNORM_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return "PVRTC1_4BPP_UNORM_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return "PVRTC2_2BPP_UNORM_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return "PVRTC2_4BPP_UNORM_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return "PVRTC1_2BPP_SRGB_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return "PVRTC1_4BPP_SRGB_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return "PVRTC2_2BPP_SRGB_BLOCK_IMG"; break;
	case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return "PVRTC2_4BPP_SRGB_BLOCK_IMG"; break;
	case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT: return "ASTC_4x4_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT: return "ASTC_5x4_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT: return "ASTC_5x5_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT: return "ASTC_6x5_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT: return "ASTC_6x6_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT: return "ASTC_8x5_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT: return "ASTC_8x6_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT: return "ASTC_8x8_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT: return "ASTC_10x5_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT: return "ASTC_10x6_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT: return "ASTC_10x8_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT: return "ASTC_10x10_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT: return "ASTC_12x10_SFLOAT_BLOCK_EXT"; break;
	case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT: return "ASTC_12x12_SFLOAT_BLOCK_EXT"; break;
	default: return "UNDEFINED"; break;
	}
	return "UNDEFINED";
}

StringView getVkColorSpaceName(VkColorSpaceKHR fmt) {
	switch (fmt) {
	case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "SRGB_NONLINEAR"; break;
	case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "DISPLAY_P3_NONLINEAR"; break;
	case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return "EXTENDED_SRGB_LINEAR"; break;
	case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT: return "DISPLAY_P3_LINEAR"; break;
	case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT: return "DCI_P3_NONLINEAR"; break;
	case VK_COLOR_SPACE_BT709_LINEAR_EXT: return "BT709_LINEAR"; break;
	case VK_COLOR_SPACE_BT709_NONLINEAR_EXT: return "BT709_NONLINEAR"; break;
	case VK_COLOR_SPACE_BT2020_LINEAR_EXT: return "BT2020_LINEAR"; break;
	case VK_COLOR_SPACE_HDR10_ST2084_EXT: return "HDR10_ST2084"; break;
	case VK_COLOR_SPACE_DOLBYVISION_EXT: return "DOLBYVISION"; break;
	case VK_COLOR_SPACE_HDR10_HLG_EXT: return "HDR10_HLG"; break;
	case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT: return "ADOBERGB_LINEAR"; break;
	case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT: return "ADOBERGB_NONLINEAR"; break;
	case VK_COLOR_SPACE_PASS_THROUGH_EXT: return "PASS_THROUGH"; break;
	case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT: return "EXTENDED_SRGB_NONLINEAR"; break;
	case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD: return "DISPLAY_NATIVE"; break;
	default: return "UNKNOWN"; break;
	}
	return "UNKNOWN";
}

String getVkMemoryPropertyFlags(VkMemoryPropertyFlags flags) {
	StringStream ret;
	if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) { ret << " DEVICE_LOCAL"; }
	if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) { ret << " HOST_VISIBLE"; }
	if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) { ret << " HOST_COHERENT"; }
	if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) { ret << " HOST_CACHED"; }
	if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) { ret << " LAZILY_ALLOCATED"; }
	if (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT) { ret << " PROTECTED"; }
	if (flags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) { ret << " DEVICE_COHERENT_AMD"; }
	if (flags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) { ret << " DEVICE_UNCACHED_AMD"; }
	return ret.str();
}

#if VK_DEBUG_LOG

static VkResult s_createDebugUtilsMessengerEXT(VkInstance instance, const PFN_vkGetInstanceProcAddr getInstanceProcAddr, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) getInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

#endif

static ExtensionFlags getFlagForExtension(const char *name) {
	if (strcmp(name, VK_KHR_MAINTENANCE3_EXTENSION_NAME) == 0) {
		return ExtensionFlags::Maintenance3;
	} else if (strcmp(name, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0) {
		return ExtensionFlags::DescriptorIndexing;
	} else if (strcmp(name, VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0) {
		return ExtensionFlags::DrawIndirectCount;
	} else if (strcmp(name, VK_KHR_16BIT_STORAGE_EXTENSION_NAME) == 0) {
		return ExtensionFlags::Storage16Bit;
	} else if (strcmp(name, VK_KHR_8BIT_STORAGE_EXTENSION_NAME) == 0) {
		return ExtensionFlags::Storage8Bit;
	} else if (strcmp(name, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0) {
		return ExtensionFlags::DeviceAddress;
	} else if (strcmp(name, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME) == 0) {
		return ExtensionFlags::ShaderInt8 | ExtensionFlags::ShaderFloat16;
	} else if (strcmp(name, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0) {
		return ExtensionFlags::MemoryBudget;
	} else if (strcmp(name, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0) {
		return ExtensionFlags::GetMemoryRequirements2;
	} else if (strcmp(name, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0) {
		return ExtensionFlags::DedicatedAllocation;
#if __APPLE__
	} else if (strcmp(name, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0) {
		return ExtensionFlags::Portability;
#endif
	}
	return ExtensionFlags::None;
}

bool checkIfExtensionAvailable(uint32_t apiVersion, const char *name, const Vector<VkExtensionProperties> &available,
		Vector<StringView> &optionals, Vector<StringView> &promoted, ExtensionFlags &flags) {
	auto flag = getFlagForExtension(name);
	if (flag == ExtensionFlags::None) {
		log::format("Vk", "Extension is not registered as optional: %s", name);
		return false;
	}

	if (apiVersion >= VK_API_VERSION_1_3) {
		for (auto &it : s_promotedVk13Extensions) {
			if (it) {
				if (strcmp(name, it) == 0) {
					flags |= flag;
					promoted.emplace_back(StringView(name));
					return true;
				}
			}
		}
	}
	if (apiVersion >= VK_API_VERSION_1_2) {
		for (auto &it : s_promotedVk12Extensions) {
			if (it) {
				if (strcmp(name, it) == 0) {
					flags |= flag;
					promoted.emplace_back(StringView(name));
					return true;
				}
			}
		}
	}
	if (apiVersion >= VK_API_VERSION_1_1) {
		for (auto &it : s_promotedVk11Extensions) {
			if (it) {
				if (strcmp(name, it) == 0) {
					flags |= flag;
					promoted.emplace_back(StringView(name));
					return true;
				}
			}
		}
	}

	for (auto &it : available) {
		if (strcmp(name, it.extensionName) == 0) {
			flags |= flag;
			optionals.emplace_back(StringView(name));
			return true;
		}
	}
	return false;
}

bool isPromotedExtension(uint32_t apiVersion, StringView name) {
	if (apiVersion >= VK_API_VERSION_1_2) {
		for (auto &it : s_promotedVk12Extensions) {
			if (it) {
				if (strcmp(name.data(), it) == 0) {
					return true;
				}
			}
		}
	}
	if (apiVersion >= VK_API_VERSION_1_1) {
		for (auto &it : s_promotedVk11Extensions) {
			if (it) {
				if (strcmp(name.data(), it) == 0) {
					return true;
				}
			}
		}
	}
	return false;
}

size_t getFormatBlockSize(VkFormat format) {
	switch (format) {
	case VK_FORMAT_UNDEFINED: return 0; break;
	case VK_FORMAT_MAX_ENUM: return 0; break;
	case VK_FORMAT_R4G4_UNORM_PACK8: return 1; break;
	case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return 2; break;
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R5G6B5_UNORM_PACK16: return 2; break;
	case VK_FORMAT_B5G6R5_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return 2; break;
	case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return 2; break;
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R8_UNORM: return 1; break;
	case VK_FORMAT_R8_SNORM: return 1; break;
	case VK_FORMAT_R8_USCALED: return 1; break;
	case VK_FORMAT_R8_SSCALED: return 1; break;
	case VK_FORMAT_R8_UINT: return 1; break;
	case VK_FORMAT_R8_SINT: return 1; break;
	case VK_FORMAT_R8_SRGB: return 1; break;
	case VK_FORMAT_R8G8_UNORM: return 2; break;
	case VK_FORMAT_R8G8_SNORM: return 2; break;
	case VK_FORMAT_R8G8_USCALED: return 2; break;
	case VK_FORMAT_R8G8_SSCALED: return 2; break;
	case VK_FORMAT_R8G8_UINT: return 2; break;
	case VK_FORMAT_R8G8_SINT: return 2; break;
	case VK_FORMAT_R8G8_SRGB: return 2; break;
	case VK_FORMAT_R8G8B8_UNORM: return 3; break;
	case VK_FORMAT_R8G8B8_SNORM: return 3; break;
	case VK_FORMAT_R8G8B8_USCALED: return 3; break;
	case VK_FORMAT_R8G8B8_SSCALED: return 3; break;
	case VK_FORMAT_R8G8B8_UINT: return 3; break;
	case VK_FORMAT_R8G8B8_SINT: return 3; break;
	case VK_FORMAT_R8G8B8_SRGB: return 3; break;
	case VK_FORMAT_B8G8R8_UNORM: return 3; break;
	case VK_FORMAT_B8G8R8_SNORM: return 3; break;
	case VK_FORMAT_B8G8R8_USCALED: return 3; break;
	case VK_FORMAT_B8G8R8_SSCALED: return 3; break;
	case VK_FORMAT_B8G8R8_UINT: return 3; break;
	case VK_FORMAT_B8G8R8_SINT: return 3; break;
	case VK_FORMAT_B8G8R8_SRGB: return 3; break;
	case VK_FORMAT_R8G8B8A8_UNORM: return 4; break;
	case VK_FORMAT_R8G8B8A8_SNORM: return 4; break;
	case VK_FORMAT_R8G8B8A8_USCALED: return 4; break;
	case VK_FORMAT_R8G8B8A8_SSCALED: return 4; break;
	case VK_FORMAT_R8G8B8A8_UINT: return 4; break;
	case VK_FORMAT_R8G8B8A8_SINT: return 4; break;
	case VK_FORMAT_R8G8B8A8_SRGB: return 4; break;
	case VK_FORMAT_B8G8R8A8_UNORM: return 4; break;
	case VK_FORMAT_B8G8R8A8_SNORM: return 4; break;
	case VK_FORMAT_B8G8R8A8_USCALED: return 4; break;
	case VK_FORMAT_B8G8R8A8_SSCALED: return 4; break;
	case VK_FORMAT_B8G8R8A8_UINT: return 4; break;
	case VK_FORMAT_B8G8R8A8_SINT: return 4; break;
	case VK_FORMAT_B8G8R8A8_SRGB: return 4; break;
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_UINT_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_SINT_PACK32: return 4; break;
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_UINT_PACK32: return 4; break;
	case VK_FORMAT_A2R10G10B10_SINT_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_UINT_PACK32: return 4; break;
	case VK_FORMAT_A2B10G10R10_SINT_PACK32: return 4; break;
	case VK_FORMAT_R16_UNORM: return 2; break;
	case VK_FORMAT_R16_SNORM: return 2; break;
	case VK_FORMAT_R16_USCALED: return 2; break;
	case VK_FORMAT_R16_SSCALED: return 2; break;
	case VK_FORMAT_R16_UINT: return 2; break;
	case VK_FORMAT_R16_SINT: return 2; break;
	case VK_FORMAT_R16_SFLOAT: return 2; break;
	case VK_FORMAT_R16G16_UNORM: return 4; break;
	case VK_FORMAT_R16G16_SNORM: return 4; break;
	case VK_FORMAT_R16G16_USCALED: return 4; break;
	case VK_FORMAT_R16G16_SSCALED: return 4; break;
	case VK_FORMAT_R16G16_UINT: return 4; break;
	case VK_FORMAT_R16G16_SINT: return 4; break;
	case VK_FORMAT_R16G16_SFLOAT: return 4; break;
	case VK_FORMAT_R16G16B16_UNORM: return 6; break;
	case VK_FORMAT_R16G16B16_SNORM: return 6; break;
	case VK_FORMAT_R16G16B16_USCALED: return 6; break;
	case VK_FORMAT_R16G16B16_SSCALED: return 6; break;
	case VK_FORMAT_R16G16B16_UINT: return 6; break;
	case VK_FORMAT_R16G16B16_SINT: return 6; break;
	case VK_FORMAT_R16G16B16_SFLOAT: return 6; break;
	case VK_FORMAT_R16G16B16A16_UNORM: return 8; break;
	case VK_FORMAT_R16G16B16A16_SNORM: return 8; break;
	case VK_FORMAT_R16G16B16A16_USCALED: return 8; break;
	case VK_FORMAT_R16G16B16A16_SSCALED: return 8; break;
	case VK_FORMAT_R16G16B16A16_UINT: return 8; break;
	case VK_FORMAT_R16G16B16A16_SINT: return 8; break;
	case VK_FORMAT_R16G16B16A16_SFLOAT: return 8; break;
	case VK_FORMAT_R32_UINT: return 4; break;
	case VK_FORMAT_R32_SINT: return 4; break;
	case VK_FORMAT_R32_SFLOAT: return 4; break;
	case VK_FORMAT_R32G32_UINT: return 8; break;
	case VK_FORMAT_R32G32_SINT: return 8; break;
	case VK_FORMAT_R32G32_SFLOAT: return 8; break;
	case VK_FORMAT_R32G32B32_UINT: return 12; break;
	case VK_FORMAT_R32G32B32_SINT: return 12; break;
	case VK_FORMAT_R32G32B32_SFLOAT: return 12; break;
	case VK_FORMAT_R32G32B32A32_UINT: return 16; break;
	case VK_FORMAT_R32G32B32A32_SINT: return 16; break;
	case VK_FORMAT_R32G32B32A32_SFLOAT: return 16; break;
	case VK_FORMAT_R64_UINT: return 8; break;
	case VK_FORMAT_R64_SINT: return 8; break;
	case VK_FORMAT_R64_SFLOAT: return 8; break;
	case VK_FORMAT_R64G64_UINT: return 16; break;
	case VK_FORMAT_R64G64_SINT: return 16; break;
	case VK_FORMAT_R64G64_SFLOAT: return 16; break;
	case VK_FORMAT_R64G64B64_UINT: return 24; break;
	case VK_FORMAT_R64G64B64_SINT: return 24; break;
	case VK_FORMAT_R64G64B64_SFLOAT: return 24; break;
	case VK_FORMAT_R64G64B64A64_UINT: return 32; break;
	case VK_FORMAT_R64G64B64A64_SINT: return 32; break;
	case VK_FORMAT_R64G64B64A64_SFLOAT: return 32; break;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return 4; break;
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return 4; break;
	case VK_FORMAT_D16_UNORM: return 2; break;
	case VK_FORMAT_X8_D24_UNORM_PACK32: return 4; break;
	case VK_FORMAT_D32_SFLOAT: return 4; break;
	case VK_FORMAT_S8_UINT: return 1; break;
	case VK_FORMAT_D16_UNORM_S8_UINT: return 3; break;
	case VK_FORMAT_D24_UNORM_S8_UINT: return 4; break;
	case VK_FORMAT_D32_SFLOAT_S8_UINT: return 5; break;
	case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_BC2_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC2_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_BC3_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC3_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_BC4_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_BC4_SNORM_BLOCK: return 8; break;
	case VK_FORMAT_BC5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC5_SNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC6H_UFLOAT_BLOCK: return 16; break;
	case VK_FORMAT_BC6H_SFLOAT_BLOCK: return 16; break;
	case VK_FORMAT_BC7_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_BC7_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return 8; break;
	case VK_FORMAT_EAC_R11_UNORM_BLOCK: return 8; break;
	case VK_FORMAT_EAC_R11_SNORM_BLOCK: return 8; break;
	case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return 16; break;
	case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return 16; break;
	case VK_FORMAT_G8B8G8R8_422_UNORM: return 4; break;
	case VK_FORMAT_B8G8R8G8_422_UNORM: return 4; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return 3; break;
	case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return 3; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return 3; break;
	case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return 3; break;
	case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return 3; break;
	case VK_FORMAT_R10X6_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return 4; break;
	case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return 4; break;
	case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_R12X4_UNORM_PACK16: return 2; break;
	case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return 4; break;
	case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return 8; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return 6; break;
	case VK_FORMAT_G16B16G16R16_422_UNORM: return 8; break;
	case VK_FORMAT_B16G16R16G16_422_UNORM: return 8; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return 6; break;
	case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return 6; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return 6; break;
	case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return 6; break;
	case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return 6; break;
	case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return 8; break;
	case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return 8; break;
	case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT: return 8; break;
	case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT: return 3; break;
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT: return 6; break;
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT: return 6; break;
	case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT: return 6; break;
	case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT: return 2; break;
	case VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT: return 2; break;
	default:
		log::vtext("Vk", "Format is not supported: ", format);
		break;
	}
	return 0;
}

VkPresentModeKHR getVkPresentMode(gl::PresentMode presentMode) {
	switch (presentMode) {
	case gl::PresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR; break;
	case gl::PresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR; break;
	case gl::PresentMode::Fifo: return VK_PRESENT_MODE_FIFO_KHR; break;
	case gl::PresentMode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR; break;
	default: break;
	}
	return VkPresentModeKHR(0);
}

}

std::ostream &operator<< (std::ostream &stream, VkResult res) {
	switch (res) {
	case VK_SUCCESS: stream << "VK_SUCCESS"; break;
	case VK_NOT_READY: stream << "VK_NOT_READY"; break;
	case VK_TIMEOUT: stream << "VK_TIMEOUT"; break;
	case VK_EVENT_SET: stream << "VK_EVENT_SET"; break;
	case VK_EVENT_RESET: stream << "VK_EVENT_RESET"; break;
	case VK_INCOMPLETE: stream << "VK_INCOMPLETE"; break;
	case VK_ERROR_OUT_OF_HOST_MEMORY: stream << "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: stream << "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
	case VK_ERROR_INITIALIZATION_FAILED: stream << "VK_ERROR_INITIALIZATION_FAILED"; break;
	case VK_ERROR_DEVICE_LOST: stream << "VK_ERROR_DEVICE_LOST"; break;
	case VK_ERROR_MEMORY_MAP_FAILED: stream << "VK_ERROR_MEMORY_MAP_FAILED"; break;
	case VK_ERROR_LAYER_NOT_PRESENT: stream << "VK_ERROR_LAYER_NOT_PRESENT"; break;
	case VK_ERROR_EXTENSION_NOT_PRESENT: stream << "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
	case VK_ERROR_FEATURE_NOT_PRESENT: stream << "VK_ERROR_FEATURE_NOT_PRESENT"; break;
	case VK_ERROR_INCOMPATIBLE_DRIVER: stream << "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
	case VK_ERROR_TOO_MANY_OBJECTS: stream << "VK_ERROR_TOO_MANY_OBJECTS"; break;
	case VK_ERROR_FORMAT_NOT_SUPPORTED: stream << "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
	case VK_ERROR_FRAGMENTED_POOL: stream << "VK_ERROR_FRAGMENTED_POOL"; break;
	case VK_ERROR_UNKNOWN: stream << "VK_ERROR_UNKNOWN"; break;
	case VK_ERROR_OUT_OF_POOL_MEMORY: stream << "VK_ERROR_OUT_OF_POOL_MEMORY"; break;
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: stream << "VK_ERROR_INVALID_EXTERNAL_HANDLE"; break;
	case VK_ERROR_FRAGMENTATION: stream << "VK_ERROR_FRAGMENTATION"; break;
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: stream << "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"; break;
	case VK_PIPELINE_COMPILE_REQUIRED: stream << "VK_PIPELINE_COMPILE_REQUIRED"; break;
	case VK_ERROR_SURFACE_LOST_KHR: stream << "VK_ERROR_SURFACE_LOST_KHR"; break;
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: stream << "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
	case VK_SUBOPTIMAL_KHR: stream << "VK_SUBOPTIMAL_KHR"; break;
	case VK_ERROR_OUT_OF_DATE_KHR: stream << "VK_ERROR_OUT_OF_DATE_KHR"; break;
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: stream << "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
	case VK_ERROR_VALIDATION_FAILED_EXT: stream << "VK_ERROR_VALIDATION_FAILED_EXT"; break;
	case VK_ERROR_INVALID_SHADER_NV: stream << "VK_ERROR_INVALID_SHADER_NV"; break;
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: stream << "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"; break;
	case VK_ERROR_NOT_PERMITTED_KHR: stream << "VK_ERROR_NOT_PERMITTED_KHR"; break;
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: stream << "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"; break;
	case VK_THREAD_IDLE_KHR: stream << "VK_THREAD_IDLE_KHR"; break;
	case VK_THREAD_DONE_KHR: stream << "VK_THREAD_DONE_KHR"; break;
	case VK_OPERATION_DEFERRED_KHR: stream << "VK_OPERATION_DEFERRED_KHR"; break;
	case VK_OPERATION_NOT_DEFERRED_KHR: stream << "VK_OPERATION_NOT_DEFERRED_KHR"; break;
	case VK_RESULT_MAX_ENUM: stream << "VK_RESULT_MAX_ENUM"; break;
	default: stream << stappler::toString("Unknown: ", res); break;
	}
	return stream;
}
