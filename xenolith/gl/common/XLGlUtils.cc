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

#include "XLGl.h"

namespace stappler::xenolith {

String MaterialInfo::description() const {
	StringStream stream;

	stream << "{" << images[0] << "," << images[1] << "," << images[2] << "," << images[3] << "},"
			<< "{" << samplers[0] << "," << samplers[1] << "," << samplers[2] << "," << samplers[3] << "},"
			<< "{" << colorModes[0].toInt() << "," << colorModes[1].toInt() << "," << colorModes[2].toInt() << "," << colorModes[3].toInt() << "},"
			<< toInt(type) << "," << pipeline.description();

	return stream.str();
}

bool MaterialInfo::hasImage(uint64_t id) const {
	for (auto &it : images) {
		if (it == id) {
			return true;
		}
	}
	return false;
}

String PipelineMaterialInfo::data() const {
	BytesView view((const uint8_t *)this, sizeof(PipelineMaterialInfo));
	return toString(
		base16::encode<Interface>(view.sub(0, sizeof(BlendInfo))), "'",
		base16::encode<Interface>(view.sub(sizeof(BlendInfo), sizeof(DepthInfo))), "'",
		base16::encode<Interface>(view.sub(sizeof(BlendInfo) + sizeof(DepthInfo), sizeof(DepthBounds))), "'",
		base16::encode<Interface>(view.sub(sizeof(BlendInfo) + sizeof(DepthInfo) + sizeof(DepthBounds), sizeof(StencilInfo))), "'",
		base16::encode<Interface>(view.sub(sizeof(BlendInfo) + sizeof(DepthInfo) + sizeof(DepthBounds) + sizeof(StencilInfo), sizeof(StencilInfo))), "'",
		base16::encode<Interface>(view.sub(sizeof(BlendInfo) + sizeof(DepthInfo) + sizeof(DepthBounds) + sizeof(StencilInfo) * 2))
	);
}

String PipelineMaterialInfo::description() const {
	StringStream stream;
	stream << "{" << blend.enabled << "," << blend.srcColor << "," << blend.dstColor << "," << blend.opColor << ","
			<< blend.srcAlpha << "," << blend.dstAlpha << "," << blend.opAlpha << "," << blend.writeMask
			<< "},{" << depth.writeEnabled << "," << depth.testEnabled << "," << depth.compare
			<< "},{" << bounds.enabled << "," << bounds.min << "," << bounds.max
			<< "},{" << stencil << "}";
	return stream.str();
}

PipelineMaterialInfo::PipelineMaterialInfo() {
	memset(this, 0, sizeof(PipelineMaterialInfo));
}

void PipelineMaterialInfo::setBlendInfo(const BlendInfo &info) {
	if (info.isEnabled()) {
		blend = info;
	} else {
		blend = BlendInfo();
		blend.writeMask = info.writeMask;
	}
}

void PipelineMaterialInfo::setDepthInfo(const DepthInfo &info) {
	if (info.testEnabled) {
		depth.testEnabled = 1;
		depth.compare = info.compare;
	} else {
		depth.testEnabled = 0;
		depth.compare = 0;
	}
	if (info.writeEnabled) {
		depth.writeEnabled = 1;
	} else {
		depth.writeEnabled = 0;
	}
}

void PipelineMaterialInfo::setDepthBounds(const DepthBounds &b) {
	if (b.enabled) {
		bounds = b;
	} else {
		bounds = DepthBounds();
	}
}

void PipelineMaterialInfo::enableStencil(const StencilInfo &info) {
	stencil = 1;
	front = info;
	back = info;
}

void PipelineMaterialInfo::enableStencil(const StencilInfo &f, const StencilInfo &b) {
	stencil = 1;
	front = f;
	back = b;
}

void PipelineMaterialInfo::disableStancil() {
	stencil = 0;
	memset(&front, 0, sizeof(StencilInfo));
	memset(&back, 0, sizeof(StencilInfo));
}

void PipelineMaterialInfo::setLineWidth(float width) {
	if (width == 0.0f) {
		memset(&lineWidth, 0, sizeof(float));
	} else {
		lineWidth = width;
	}
}

void PipelineMaterialInfo::_setup(const BlendInfo &info) {
	setBlendInfo(info);
}

void PipelineMaterialInfo::_setup(const DepthInfo &info) {
	setDepthInfo(info);
}

void PipelineMaterialInfo::_setup(const DepthBounds &bounds) {
	setDepthBounds(bounds);
}

void PipelineMaterialInfo::_setup(const StencilInfo &info) {
	enableStencil(info);
}

void PipelineMaterialInfo::_setup(LineWidth width) {
	setLineWidth(width.get());
}

}

namespace stappler::xenolith::gl {

String getBufferFlagsDescription(BufferFlags fmt) {
	StringStream stream;
	if ((fmt & BufferFlags::SparceBinding) != BufferFlags::None) { stream << " SparceBinding"; }
	if ((fmt & BufferFlags::SparceResidency) != BufferFlags::None) { stream << " SparceResidency"; }
	if ((fmt & BufferFlags::SparceAliased) != BufferFlags::None) { stream << " SparceAliased"; }
	if ((fmt & BufferFlags::Protected) != BufferFlags::None) { stream << " Protected"; }
	return stream.str();
}

String getBufferUsageDescription(BufferUsage fmt) {
	StringStream stream;
	if ((fmt & BufferUsage::TransferSrc) != BufferUsage::None) { stream << " TransferSrc"; }
	if ((fmt & BufferUsage::TransferDst) != BufferUsage::None) { stream << " TransferDst"; }
	if ((fmt & BufferUsage::UniformTexelBuffer) != BufferUsage::None) { stream << " UniformTexelBuffer"; }
	if ((fmt & BufferUsage::StorageTexelBuffer) != BufferUsage::None) { stream << " StorageTexelBuffer"; }
	if ((fmt & BufferUsage::UniformBuffer) != BufferUsage::None) { stream << " UniformBuffer"; }
	if ((fmt & BufferUsage::StorageBuffer) != BufferUsage::None) { stream << " StorageBuffer"; }
	if ((fmt & BufferUsage::IndexBuffer) != BufferUsage::None) { stream << " IndexBuffer"; }
	if ((fmt & BufferUsage::VertexBuffer) != BufferUsage::None) { stream << " VertexBuffer"; }
	if ((fmt & BufferUsage::IndirectBuffer) != BufferUsage::None) { stream << " IndirectBuffer"; }
	if ((fmt & BufferUsage::ShaderDeviceAddress) != BufferUsage::None) { stream << " ShaderDeviceAddress"; }
	if ((fmt & BufferUsage::TransformFeedback) != BufferUsage::None) { stream << " TransformFeedback"; }
	if ((fmt & BufferUsage::TransformFeedbackCounter) != BufferUsage::None) { stream << " TransformFeedbackCounter"; }
	if ((fmt & BufferUsage::ConditionalRendering) != BufferUsage::None) { stream << " ConditionalRendering"; }
	if ((fmt & BufferUsage::AccelerationStructureBuildInputReadOnly) != BufferUsage::None) { stream << " AccelerationStructureBuildInputReadOnly"; }
	if ((fmt & BufferUsage::AccelerationStructureStorage) != BufferUsage::None) { stream << " AccelerationStructureStorage"; }
	if ((fmt & BufferUsage::ShaderBindingTable) != BufferUsage::None) { stream << " ShaderBindingTable"; }
	return stream.str();
}

String getImageFlagsDescription(ImageFlags fmt) {
	StringStream stream;
	if ((fmt & ImageFlags::SparceBinding) != ImageFlags::None) { stream << " SparceBinding"; }
	if ((fmt & ImageFlags::SparceResidency) != ImageFlags::None) { stream << " SparceResidency"; }
	if ((fmt & ImageFlags::SparceAliased) != ImageFlags::None) { stream << " SparceAliased"; }
	if ((fmt & ImageFlags::MutableFormat) != ImageFlags::None) { stream << " MutableFormat"; }
	if ((fmt & ImageFlags::CubeCompatible) != ImageFlags::None) { stream << " CubeCompatible"; }
	if ((fmt & ImageFlags::Alias) != ImageFlags::None) { stream << " Alias"; }
	if ((fmt & ImageFlags::SplitInstanceBindRegions) != ImageFlags::None) { stream << " SplitInstanceBindRegions"; }
	if ((fmt & ImageFlags::Array2dCompatible) != ImageFlags::None) { stream << " Array2dCompatible"; }
	if ((fmt & ImageFlags::BlockTexelViewCompatible) != ImageFlags::None) { stream << " BlockTexelViewCompatible"; }
	if ((fmt & ImageFlags::ExtendedUsage) != ImageFlags::None) { stream << " ExtendedUsage"; }
	if ((fmt & ImageFlags::Protected) != ImageFlags::None) { stream << " Protected"; }
	if ((fmt & ImageFlags::Disjoint) != ImageFlags::None) { stream << " Disjoint"; }
	return stream.str();
}

String getSampleCountDescription(SampleCount fmt) {
	StringStream stream;
	if ((fmt & SampleCount::X1) != SampleCount::None) { stream << " x1"; }
	if ((fmt & SampleCount::X2) != SampleCount::None) { stream << " x2"; }
	if ((fmt & SampleCount::X4) != SampleCount::None) { stream << " x4"; }
	if ((fmt & SampleCount::X8) != SampleCount::None) { stream << " x8"; }
	if ((fmt & SampleCount::X16) != SampleCount::None) { stream << " x16"; }
	if ((fmt & SampleCount::X32) != SampleCount::None) { stream << " x32"; }
	if ((fmt & SampleCount::X64) != SampleCount::None) { stream << " x64"; }
	return stream.str();
}

StringView getImageTypeName(ImageType type) {
	switch (type) {
	case ImageType::Image1D: return StringView("1D"); break;
	case ImageType::Image2D: return StringView("2D"); break;
	case ImageType::Image3D: return StringView("3D"); break;
	}
	return StringView("Unknown");
}

StringView getImageViewTypeName(ImageViewType type) {
	switch (type) {
	case ImageViewType::ImageView1D: return StringView("1D"); break;
	case ImageViewType::ImageView1DArray: return StringView("1DArray"); break;
	case ImageViewType::ImageView2D: return StringView("2D"); break;
	case ImageViewType::ImageView2DArray: return StringView("2DArray"); break;
	case ImageViewType::ImageView3D: return StringView("3D"); break;
	case ImageViewType::ImageViewCube: return StringView("Cube"); break;
	case ImageViewType::ImageViewCubeArray: return StringView("CubeArray"); break;
	}
	return StringView("Unknown");
}

StringView getImageFormatName(ImageFormat fmt) {
	switch (fmt) {
	case ImageFormat::Undefined: return StringView("Undefined"); break;
	case ImageFormat::R4G4_UNORM_PACK8: return StringView("R4G4_UNORM_PACK8"); break;
	case ImageFormat::R4G4B4A4_UNORM_PACK16: return StringView("R4G4B4A4_UNORM_PACK16"); break;
	case ImageFormat::B4G4R4A4_UNORM_PACK16: return StringView("B4G4R4A4_UNORM_PACK16"); break;
	case ImageFormat::R5G6B5_UNORM_PACK16: return StringView("R5G6B5_UNORM_PACK16"); break;
	case ImageFormat::B5G6R5_UNORM_PACK16: return StringView("B5G6R5_UNORM_PACK16"); break;
	case ImageFormat::R5G5B5A1_UNORM_PACK16: return StringView("R5G5B5A1_UNORM_PACK16"); break;
	case ImageFormat::B5G5R5A1_UNORM_PACK16: return StringView("B5G5R5A1_UNORM_PACK16"); break;
	case ImageFormat::A1R5G5B5_UNORM_PACK16: return StringView("A1R5G5B5_UNORM_PACK16"); break;
	case ImageFormat::R8_UNORM: return StringView("R8_UNORM"); break;
	case ImageFormat::R8_SNORM: return StringView("R8_SNORM"); break;
	case ImageFormat::R8_USCALED: return StringView("R8_USCALED"); break;
	case ImageFormat::R8_SSCALED: return StringView("R8_SSCALED"); break;
	case ImageFormat::R8_UINT: return StringView("R8_UINT"); break;
	case ImageFormat::R8_SINT: return StringView("R8_SINT"); break;
	case ImageFormat::R8_SRGB: return StringView("R8_SRGB"); break;
	case ImageFormat::R8G8_UNORM: return StringView("R8G8_UNORM"); break;
	case ImageFormat::R8G8_SNORM: return StringView("R8G8_SNORM"); break;
	case ImageFormat::R8G8_USCALED: return StringView("R8G8_USCALED"); break;
	case ImageFormat::R8G8_SSCALED: return StringView("R8G8_SSCALED"); break;
	case ImageFormat::R8G8_UINT: return StringView("R8G8_UINT"); break;
	case ImageFormat::R8G8_SINT: return StringView("R8G8_SINT"); break;
	case ImageFormat::R8G8_SRGB: return StringView("R8G8_SRGB"); break;
	case ImageFormat::R8G8B8_UNORM: return StringView("R8G8B8_UNORM"); break;
	case ImageFormat::R8G8B8_SNORM: return StringView("R8G8B8_SNORM"); break;
	case ImageFormat::R8G8B8_USCALED: return StringView("R8G8B8_USCALED"); break;
	case ImageFormat::R8G8B8_SSCALED: return StringView("R8G8B8_SSCALED"); break;
	case ImageFormat::R8G8B8_UINT: return StringView("R8G8B8_UINT"); break;
	case ImageFormat::R8G8B8_SINT: return StringView("R8G8B8_SINT"); break;
	case ImageFormat::R8G8B8_SRGB: return StringView("R8G8B8_SRGB"); break;
	case ImageFormat::B8G8R8_UNORM: return StringView("B8G8R8_UNORM"); break;
	case ImageFormat::B8G8R8_SNORM: return StringView("B8G8R8_SNORM"); break;
	case ImageFormat::B8G8R8_USCALED: return StringView("B8G8R8_USCALED"); break;
	case ImageFormat::B8G8R8_SSCALED: return StringView("B8G8R8_SSCALED"); break;
	case ImageFormat::B8G8R8_UINT: return StringView("B8G8R8_UINT"); break;
	case ImageFormat::B8G8R8_SINT: return StringView("B8G8R8_SINT"); break;
	case ImageFormat::B8G8R8_SRGB: return StringView("B8G8R8_SRGB"); break;
	case ImageFormat::R8G8B8A8_UNORM: return StringView("R8G8B8A8_UNORM"); break;
	case ImageFormat::R8G8B8A8_SNORM: return StringView("R8G8B8A8_SNORM"); break;
	case ImageFormat::R8G8B8A8_USCALED: return StringView("R8G8B8A8_USCALED"); break;
	case ImageFormat::R8G8B8A8_SSCALED: return StringView("R8G8B8A8_SSCALED"); break;
	case ImageFormat::R8G8B8A8_UINT: return StringView("R8G8B8A8_UINT"); break;
	case ImageFormat::R8G8B8A8_SINT: return StringView("R8G8B8A8_SINT"); break;
	case ImageFormat::R8G8B8A8_SRGB: return StringView("R8G8B8A8_SRGB"); break;
	case ImageFormat::B8G8R8A8_UNORM: return StringView("B8G8R8A8_UNORM"); break;
	case ImageFormat::B8G8R8A8_SNORM: return StringView("B8G8R8A8_SNORM"); break;
	case ImageFormat::B8G8R8A8_USCALED: return StringView("B8G8R8A8_USCALED"); break;
	case ImageFormat::B8G8R8A8_SSCALED: return StringView("B8G8R8A8_SSCALED"); break;
	case ImageFormat::B8G8R8A8_UINT: return StringView("B8G8R8A8_UINT"); break;
	case ImageFormat::B8G8R8A8_SINT: return StringView("B8G8R8A8_SINT"); break;
	case ImageFormat::B8G8R8A8_SRGB: return StringView("B8G8R8A8_SRGB"); break;
	case ImageFormat::A8B8G8R8_UNORM_PACK32: return StringView("A8B8G8R8_UNORM_PACK32"); break;
	case ImageFormat::A8B8G8R8_SNORM_PACK32: return StringView("A8B8G8R8_SNORM_PACK32"); break;
	case ImageFormat::A8B8G8R8_USCALED_PACK32: return StringView("A8B8G8R8_USCALED_PACK32"); break;
	case ImageFormat::A8B8G8R8_SSCALED_PACK32: return StringView("A8B8G8R8_SSCALED_PACK32"); break;
	case ImageFormat::A8B8G8R8_UINT_PACK32: return StringView("A8B8G8R8_UINT_PACK32"); break;
	case ImageFormat::A8B8G8R8_SINT_PACK32: return StringView("A8B8G8R8_SINT_PACK32"); break;
	case ImageFormat::A8B8G8R8_SRGB_PACK32: return StringView("A8B8G8R8_SRGB_PACK32"); break;
	case ImageFormat::A2R10G10B10_UNORM_PACK32: return StringView("A2R10G10B10_UNORM_PACK32"); break;
	case ImageFormat::A2R10G10B10_SNORM_PACK32: return StringView("A2R10G10B10_SNORM_PACK32"); break;
	case ImageFormat::A2R10G10B10_USCALED_PACK32: return StringView("A2R10G10B10_USCALED_PACK32"); break;
	case ImageFormat::A2R10G10B10_SSCALED_PACK32: return StringView("A2R10G10B10_SSCALED_PACK32"); break;
	case ImageFormat::A2R10G10B10_UINT_PACK32: return StringView("A2R10G10B10_UINT_PACK32"); break;
	case ImageFormat::A2R10G10B10_SINT_PACK32: return StringView("A2R10G10B10_SINT_PACK32"); break;
	case ImageFormat::A2B10G10R10_UNORM_PACK32: return StringView("A2B10G10R10_UNORM_PACK32"); break;
	case ImageFormat::A2B10G10R10_SNORM_PACK32: return StringView("A2B10G10R10_SNORM_PACK32"); break;
	case ImageFormat::A2B10G10R10_USCALED_PACK32: return StringView("A2B10G10R10_USCALED_PACK32"); break;
	case ImageFormat::A2B10G10R10_SSCALED_PACK32: return StringView("A2B10G10R10_SSCALED_PACK32"); break;
	case ImageFormat::A2B10G10R10_UINT_PACK32: return StringView("A2B10G10R10_UINT_PACK32"); break;
	case ImageFormat::A2B10G10R10_SINT_PACK32: return StringView("A2B10G10R10_SINT_PACK32"); break;
	case ImageFormat::R16_UNORM: return StringView("R16_UNORM"); break;
	case ImageFormat::R16_SNORM: return StringView("R16_SNORM"); break;
	case ImageFormat::R16_USCALED: return StringView("R16_USCALED"); break;
	case ImageFormat::R16_SSCALED: return StringView("R16_SSCALED"); break;
	case ImageFormat::R16_UINT: return StringView("R16_UINT"); break;
	case ImageFormat::R16_SINT: return StringView("R16_SINT"); break;
	case ImageFormat::R16_SFLOAT: return StringView("R16_SFLOAT"); break;
	case ImageFormat::R16G16_UNORM: return StringView("R16G16_UNORM"); break;
	case ImageFormat::R16G16_SNORM: return StringView("R16G16_SNORM"); break;
	case ImageFormat::R16G16_USCALED: return StringView("R16G16_USCALED"); break;
	case ImageFormat::R16G16_SSCALED: return StringView("R16G16_SSCALED"); break;
	case ImageFormat::R16G16_UINT: return StringView("R16G16_UINT"); break;
	case ImageFormat::R16G16_SINT: return StringView("R16G16_SINT"); break;
	case ImageFormat::R16G16_SFLOAT: return StringView("R16G16_SFLOAT"); break;
	case ImageFormat::R16G16B16_UNORM: return StringView("R16G16B16_UNORM"); break;
	case ImageFormat::R16G16B16_SNORM: return StringView("R16G16B16_SNORM"); break;
	case ImageFormat::R16G16B16_USCALED: return StringView("R16G16B16_USCALED"); break;
	case ImageFormat::R16G16B16_SSCALED: return StringView("R16G16B16_SSCALED"); break;
	case ImageFormat::R16G16B16_UINT: return StringView("R16G16B16_UINT"); break;
	case ImageFormat::R16G16B16_SINT: return StringView("R16G16B16_SINT"); break;
	case ImageFormat::R16G16B16_SFLOAT: return StringView("R16G16B16_SFLOAT"); break;
	case ImageFormat::R16G16B16A16_UNORM: return StringView("R16G16B16A16_UNORM"); break;
	case ImageFormat::R16G16B16A16_SNORM: return StringView("R16G16B16A16_SNORM"); break;
	case ImageFormat::R16G16B16A16_USCALED: return StringView("R16G16B16A16_USCALED"); break;
	case ImageFormat::R16G16B16A16_SSCALED: return StringView("R16G16B16A16_SSCALED"); break;
	case ImageFormat::R16G16B16A16_UINT: return StringView("R16G16B16A16_UINT"); break;
	case ImageFormat::R16G16B16A16_SINT: return StringView("R16G16B16A16_SINT"); break;
	case ImageFormat::R16G16B16A16_SFLOAT: return StringView("R16G16B16A16_SFLOAT"); break;
	case ImageFormat::R32_UINT: return StringView("R32_UINT"); break;
	case ImageFormat::R32_SINT: return StringView("R32_SINT"); break;
	case ImageFormat::R32_SFLOAT: return StringView("R32_SFLOAT"); break;
	case ImageFormat::R32G32_UINT: return StringView("R32G32_UINT"); break;
	case ImageFormat::R32G32_SINT: return StringView("R32G32_SINT"); break;
	case ImageFormat::R32G32_SFLOAT: return StringView("R32G32_SFLOAT"); break;
	case ImageFormat::R32G32B32_UINT: return StringView("R32G32B32_UINT"); break;
	case ImageFormat::R32G32B32_SINT: return StringView("R32G32B32_SINT"); break;
	case ImageFormat::R32G32B32_SFLOAT: return StringView("R32G32B32_SFLOAT"); break;
	case ImageFormat::R32G32B32A32_UINT: return StringView("R32G32B32A32_UINT"); break;
	case ImageFormat::R32G32B32A32_SINT: return StringView("R32G32B32A32_SINT"); break;
	case ImageFormat::R32G32B32A32_SFLOAT: return StringView("R32G32B32A32_SFLOAT"); break;
	case ImageFormat::R64_UINT: return StringView("R64_UINT"); break;
	case ImageFormat::R64_SINT: return StringView("R64_SINT"); break;
	case ImageFormat::R64_SFLOAT: return StringView("R64_SFLOAT"); break;
	case ImageFormat::R64G64_UINT: return StringView("R64G64_UINT"); break;
	case ImageFormat::R64G64_SINT: return StringView("R64G64_SINT"); break;
	case ImageFormat::R64G64_SFLOAT: return StringView("R64G64_SFLOAT"); break;
	case ImageFormat::R64G64B64_UINT: return StringView("R64G64B64_UINT"); break;
	case ImageFormat::R64G64B64_SINT: return StringView("R64G64B64_SINT"); break;
	case ImageFormat::R64G64B64_SFLOAT: return StringView("R64G64B64_SFLOAT"); break;
	case ImageFormat::R64G64B64A64_UINT: return StringView("R64G64B64A64_UINT"); break;
	case ImageFormat::R64G64B64A64_SINT: return StringView("R64G64B64A64_SINT"); break;
	case ImageFormat::R64G64B64A64_SFLOAT: return StringView("R64G64B64A64_SFLOAT"); break;
	case ImageFormat::B10G11R11_UFLOAT_PACK32: return StringView("B10G11R11_UFLOAT_PACK32"); break;
	case ImageFormat::E5B9G9R9_UFLOAT_PACK32: return StringView("E5B9G9R9_UFLOAT_PACK32"); break;
	case ImageFormat::D16_UNORM: return StringView("D16_UNORM"); break;
	case ImageFormat::X8_D24_UNORM_PACK32: return StringView("X8_D24_UNORM_PACK32"); break;
	case ImageFormat::D32_SFLOAT: return StringView("D32_SFLOAT"); break;
	case ImageFormat::S8_UINT: return StringView("S8_UINT"); break;
	case ImageFormat::D16_UNORM_S8_UINT: return StringView("D16_UNORM_S8_UINT"); break;
	case ImageFormat::D24_UNORM_S8_UINT: return StringView("D24_UNORM_S8_UINT"); break;
	case ImageFormat::D32_SFLOAT_S8_UINT: return StringView("D32_SFLOAT_S8_UINT"); break;
	case ImageFormat::BC1_RGB_UNORM_BLOCK: return StringView("BC1_RGB_UNORM_BLOCK"); break;
	case ImageFormat::BC1_RGB_SRGB_BLOCK: return StringView("BC1_RGB_SRGB_BLOCK"); break;
	case ImageFormat::BC1_RGBA_UNORM_BLOCK: return StringView("BC1_RGBA_UNORM_BLOCK"); break;
	case ImageFormat::BC1_RGBA_SRGB_BLOCK: return StringView("BC1_RGBA_SRGB_BLOCK"); break;
	case ImageFormat::BC2_UNORM_BLOCK: return StringView("BC2_UNORM_BLOCK"); break;
	case ImageFormat::BC2_SRGB_BLOCK: return StringView("BC2_SRGB_BLOCK"); break;
	case ImageFormat::BC3_UNORM_BLOCK: return StringView("BC3_UNORM_BLOCK"); break;
	case ImageFormat::BC3_SRGB_BLOCK: return StringView("BC3_SRGB_BLOCK"); break;
	case ImageFormat::BC4_UNORM_BLOCK: return StringView("BC4_UNORM_BLOCK"); break;
	case ImageFormat::BC4_SNORM_BLOCK: return StringView("BC4_SNORM_BLOCK"); break;
	case ImageFormat::BC5_UNORM_BLOCK: return StringView("BC5_UNORM_BLOCK"); break;
	case ImageFormat::BC5_SNORM_BLOCK: return StringView("BC5_SNORM_BLOCK"); break;
	case ImageFormat::BC6H_UFLOAT_BLOCK: return StringView("BC6H_UFLOAT_BLOCK"); break;
	case ImageFormat::BC6H_SFLOAT_BLOCK: return StringView("BC6H_SFLOAT_BLOCK"); break;
	case ImageFormat::BC7_UNORM_BLOCK: return StringView("BC7_UNORM_BLOCK"); break;
	case ImageFormat::BC7_SRGB_BLOCK: return StringView("BC7_SRGB_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8_UNORM_BLOCK: return StringView("ETC2_R8G8B8_UNORM_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8_SRGB_BLOCK: return StringView("ETC2_R8G8B8_SRGB_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8A1_UNORM_BLOCK: return StringView("ETC2_R8G8B8A1_UNORM_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8A1_SRGB_BLOCK: return StringView("ETC2_R8G8B8A1_SRGB_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8A8_UNORM_BLOCK: return StringView("ETC2_R8G8B8A8_UNORM_BLOCK"); break;
	case ImageFormat::ETC2_R8G8B8A8_SRGB_BLOCK: return StringView("ETC2_R8G8B8A8_SRGB_BLOCK"); break;
	case ImageFormat::EAC_R11_UNORM_BLOCK: return StringView("EAC_R11_UNORM_BLOCK"); break;
	case ImageFormat::EAC_R11_SNORM_BLOCK: return StringView("EAC_R11_SNORM_BLOCK"); break;
	case ImageFormat::EAC_R11G11_UNORM_BLOCK: return StringView("EAC_R11G11_UNORM_BLOCK"); break;
	case ImageFormat::EAC_R11G11_SNORM_BLOCK: return StringView("EAC_R11G11_SNORM_BLOCK"); break;
	case ImageFormat::ASTC_4x4_UNORM_BLOCK: return StringView("ASTC_4x4_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_4x4_SRGB_BLOCK: return StringView("ASTC_4x4_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_5x4_UNORM_BLOCK: return StringView("ASTC_5x4_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_5x4_SRGB_BLOCK: return StringView("ASTC_5x4_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_5x5_UNORM_BLOCK: return StringView("ASTC_5x5_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_5x5_SRGB_BLOCK: return StringView("ASTC_5x5_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_6x5_UNORM_BLOCK: return StringView("ASTC_6x5_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_6x5_SRGB_BLOCK: return StringView("ASTC_6x5_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_6x6_UNORM_BLOCK: return StringView("ASTC_6x6_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_6x6_SRGB_BLOCK: return StringView("ASTC_6x6_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_8x5_UNORM_BLOCK: return StringView("ASTC_8x5_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_8x5_SRGB_BLOCK: return StringView("ASTC_8x5_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_8x6_UNORM_BLOCK: return StringView("ASTC_8x6_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_8x6_SRGB_BLOCK: return StringView("ASTC_8x6_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_8x8_UNORM_BLOCK: return StringView("ASTC_8x8_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_8x8_SRGB_BLOCK: return StringView("ASTC_8x8_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_10x5_UNORM_BLOCK: return StringView("ASTC_10x5_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_10x5_SRGB_BLOCK: return StringView("ASTC_10x5_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_10x6_UNORM_BLOCK: return StringView("ASTC_10x6_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_10x6_SRGB_BLOCK: return StringView("ASTC_10x6_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_10x8_UNORM_BLOCK: return StringView("ASTC_10x8_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_10x8_SRGB_BLOCK: return StringView("ASTC_10x8_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_10x10_UNORM_BLOCK: return StringView("ASTC_10x10_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_10x10_SRGB_BLOCK: return StringView("ASTC_10x10_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_12x10_UNORM_BLOCK: return StringView("ASTC_12x10_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_12x10_SRGB_BLOCK: return StringView("ASTC_12x10_SRGB_BLOCK"); break;
	case ImageFormat::ASTC_12x12_UNORM_BLOCK: return StringView("ASTC_12x12_UNORM_BLOCK"); break;
	case ImageFormat::ASTC_12x12_SRGB_BLOCK: return StringView("ASTC_12x12_SRGB_BLOCK"); break;
	case ImageFormat::G8B8G8R8_422_UNORM: return StringView("G8B8G8R8_422_UNORM"); break;
	case ImageFormat::B8G8R8G8_422_UNORM: return StringView("B8G8R8G8_422_UNORM"); break;
	case ImageFormat::G8_B8_R8_3PLANE_420_UNORM: return StringView("G8_B8_R8_3PLANE_420_UNORM"); break;
	case ImageFormat::G8_B8R8_2PLANE_420_UNORM: return StringView("G8_B8R8_2PLANE_420_UNORM"); break;
	case ImageFormat::G8_B8_R8_3PLANE_422_UNORM: return StringView("G8_B8_R8_3PLANE_422_UNORM"); break;
	case ImageFormat::G8_B8R8_2PLANE_422_UNORM: return StringView("G8_B8R8_2PLANE_422_UNORM"); break;
	case ImageFormat::G8_B8_R8_3PLANE_444_UNORM: return StringView("G8_B8_R8_3PLANE_444_UNORM"); break;
	case ImageFormat::R10X6_UNORM_PACK16: return StringView("R10X6_UNORM_PACK16"); break;
	case ImageFormat::R10X6G10X6_UNORM_2PACK16: return StringView("R10X6G10X6_UNORM_2PACK16"); break;
	case ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16: return StringView("R10X6G10X6B10X6A10X6_UNORM_4PACK16"); break;
	case ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return StringView("G10X6B10X6G10X6R10X6_422_UNORM_4PACK16"); break;
	case ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return StringView("B10X6G10X6R10X6G10X6_422_UNORM_4PACK16"); break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return StringView("G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16"); break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return StringView("G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16"); break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return StringView("G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16"); break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return StringView("G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16"); break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return StringView("G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16"); break;
	case ImageFormat::R12X4_UNORM_PACK16: return StringView("R12X4_UNORM_PACK16"); break;
	case ImageFormat::R12X4G12X4_UNORM_2PACK16: return StringView("R12X4G12X4_UNORM_2PACK16"); break;
	case ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16: return StringView("R12X4G12X4B12X4A12X4_UNORM_4PACK16"); break;
	case ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return StringView("G12X4B12X4G12X4R12X4_422_UNORM_4PACK16"); break;
	case ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return StringView("B12X4G12X4R12X4G12X4_422_UNORM_4PACK16"); break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return StringView("G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16"); break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return StringView("G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16"); break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return StringView("G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16"); break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return StringView("G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16"); break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return StringView("G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16"); break;
	case ImageFormat::G16B16G16R16_422_UNORM: return StringView("G16B16G16R16_422_UNORM"); break;
	case ImageFormat::B16G16R16G16_422_UNORM: return StringView("B16G16R16G16_422_UNORM"); break;
	case ImageFormat::G16_B16_R16_3PLANE_420_UNORM: return StringView("G16_B16_R16_3PLANE_420_UNORM"); break;
	case ImageFormat::G16_B16R16_2PLANE_420_UNORM: return StringView("G16_B16R16_2PLANE_420_UNORM"); break;
	case ImageFormat::G16_B16_R16_3PLANE_422_UNORM: return StringView("G16_B16_R16_3PLANE_422_UNORM"); break;
	case ImageFormat::G16_B16R16_2PLANE_422_UNORM: return StringView("G16_B16R16_2PLANE_422_UNORM"); break;
	case ImageFormat::G16_B16_R16_3PLANE_444_UNORM: return StringView("G16_B16_R16_3PLANE_444_UNORM"); break;
	case ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG: return StringView("PVRTC1_2BPP_UNORM_BLOCK_IMG"); break;
	case ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG: return StringView("PVRTC1_4BPP_UNORM_BLOCK_IMG"); break;
	case ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG: return StringView("PVRTC2_2BPP_UNORM_BLOCK_IMG"); break;
	case ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG: return StringView("PVRTC2_4BPP_UNORM_BLOCK_IMG"); break;
	case ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG: return StringView("PVRTC1_2BPP_SRGB_BLOCK_IMG"); break;
	case ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG: return StringView("PVRTC1_4BPP_SRGB_BLOCK_IMG"); break;
	case ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG: return StringView("PVRTC2_2BPP_SRGB_BLOCK_IMG"); break;
	case ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG: return StringView("PVRTC2_4BPP_SRGB_BLOCK_IMG"); break;
	case ImageFormat::ASTC_4x4_SFLOAT_BLOCK_EXT: return StringView("ASTC_4x4_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_5x4_SFLOAT_BLOCK_EXT: return StringView("ASTC_5x4_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_5x5_SFLOAT_BLOCK_EXT: return StringView("ASTC_5x5_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_6x5_SFLOAT_BLOCK_EXT: return StringView("ASTC_6x5_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_6x6_SFLOAT_BLOCK_EXT: return StringView("ASTC_6x6_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_8x5_SFLOAT_BLOCK_EXT: return StringView("ASTC_8x5_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_8x6_SFLOAT_BLOCK_EXT: return StringView("ASTC_8x6_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_8x8_SFLOAT_BLOCK_EXT: return StringView("ASTC_8x8_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_10x5_SFLOAT_BLOCK_EXT: return StringView("ASTC_10x5_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_10x6_SFLOAT_BLOCK_EXT: return StringView("ASTC_10x6_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_10x8_SFLOAT_BLOCK_EXT: return StringView("ASTC_10x8_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_10x10_SFLOAT_BLOCK_EXT: return StringView("ASTC_10x10_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_12x10_SFLOAT_BLOCK_EXT: return StringView("ASTC_12x10_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::ASTC_12x12_SFLOAT_BLOCK_EXT: return StringView("ASTC_12x12_SFLOAT_BLOCK_EXT"); break;
	case ImageFormat::G8_B8R8_2PLANE_444_UNORM_EXT: return StringView("G8_B8R8_2PLANE_444_UNORM_EXT"); break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT: return StringView("G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT"); break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT: return StringView("G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT"); break;
	case ImageFormat::G16_B16R16_2PLANE_444_UNORM_EXT: return StringView("G16_B16R16_2PLANE_444_UNORM_EXT"); break;
	case ImageFormat::A4R4G4B4_UNORM_PACK16_EXT: return StringView("A4R4G4B4_UNORM_PACK16_EXT"); break;
	case ImageFormat::A4B4G4R4_UNORM_PACK16_EXT: return StringView("A4B4G4R4_UNORM_PACK16_EXT"); break;
	}
	return StringView("Unknown");
}

StringView getImageTilingName(ImageTiling type) {
	switch (type) {
	case ImageTiling::Optimal: return StringView("Optimal"); break;
	case ImageTiling::Linear: return StringView("Linear"); break;
	}
	return StringView("Unknown");
}

StringView getComponentMappingName(ComponentMapping mapping) {
	switch (mapping) {
	case ComponentMapping::Identity: return StringView("Id"); break;
	case ComponentMapping::Zero: return StringView("0"); break;
	case ComponentMapping::One: return StringView("1"); break;
	case ComponentMapping::R: return StringView("R"); break;
	case ComponentMapping::G: return StringView("G"); break;
	case ComponentMapping::B: return StringView("B"); break;
	case ComponentMapping::A: return StringView("A"); break;
	}
	return StringView("Unknown");
}

StringView getPresentModeName(PresentMode mode) {
	switch (mode) {
	case gl::PresentMode::Immediate: return "IMMEDIATE"; break;
	case gl::PresentMode::Mailbox: return "MAILBOX"; break;
	case gl::PresentMode::Fifo: return "FIFO"; break;
	case gl::PresentMode::FifoRelaxed: return "FIFO_RELAXED"; break;
	default: return "UNKNOWN"; break;
	}
	return StringView();
}

StringView getColorSpaceName(ColorSpace fmt) {
	switch (fmt) {
	case ColorSpace::SRGB_NONLINEAR_KHR: return StringView("SRGB_NONLINEAR_KHR"); break;
	case ColorSpace::DISPLAY_P3_NONLINEAR_EXT: return StringView("DISPLAY_P3_NONLINEAR_EXT"); break;
	case ColorSpace::EXTENDED_SRGB_LINEAR_EXT: return StringView("EXTENDED_SRGB_LINEAR_EXT"); break;
	case ColorSpace::DISPLAY_P3_LINEAR_EXT: return StringView("DISPLAY_P3_LINEAR_EXT"); break;
	case ColorSpace::DCI_P3_NONLINEAR_EXT: return StringView("DCI_P3_NONLINEAR_EXT"); break;
	case ColorSpace::BT709_LINEAR_EXT: return StringView("BT709_LINEAR_EXT"); break;
	case ColorSpace::BT709_NONLINEAR_EXT: return StringView("BT709_NONLINEAR_EXT"); break;
	case ColorSpace::BT2020_LINEAR_EXT: return StringView("BT2020_LINEAR_EXT"); break;
	case ColorSpace::HDR10_ST2084_EXT: return StringView("HDR10_ST2084_EXT"); break;
	case ColorSpace::DOLBYVISION_EXT: return StringView("DOLBYVISION_EXT"); break;
	case ColorSpace::HDR10_HLG_EXT: return StringView("HDR10_HLG_EXT"); break;
	case ColorSpace::ADOBERGB_LINEAR_EXT: return StringView("ADOBERGB_LINEAR_EXT"); break;
	case ColorSpace::ADOBERGB_NONLINEAR_EXT: return StringView("ADOBERGB_NONLINEAR_EXT"); break;
	case ColorSpace::PASS_THROUGH_EXT: return StringView("PASS_THROUGH_EXT"); break;
	case ColorSpace::EXTENDED_SRGB_NONLINEAR_EXT: return StringView("EXTENDED_SRGB_NONLINEAR_EXT"); break;
	case ColorSpace::DISPLAY_NATIVE_AMD: return StringView("DISPLAY_NATIVE_AMD"); break;
	}
	return StringView();
}

String getCompositeAlphaFlagsDescription(CompositeAlphaFlags fmt) {
	StringStream stream;
	if ((fmt & CompositeAlphaFlags::Opaque) != CompositeAlphaFlags::None) { stream << " Opaque"; }
	if ((fmt & CompositeAlphaFlags::Premultiplied) != CompositeAlphaFlags::None) { stream << " Premultiplied"; }
	if ((fmt & CompositeAlphaFlags::Postmultiplied) != CompositeAlphaFlags::None) { stream << " Postmultiplied"; }
	return stream.str();
}

String getSurfaceTransformFlagsDescription(SurfaceTransformFlags fmt) {
	StringStream stream;
	if ((fmt & SurfaceTransformFlags::Identity) != SurfaceTransformFlags::None) { stream << " Identity"; }
	if ((fmt & SurfaceTransformFlags::Rotate90) != SurfaceTransformFlags::None) { stream << " Rotate90"; }
	if ((fmt & SurfaceTransformFlags::Rotate180) != SurfaceTransformFlags::None) { stream << " Rotate180"; }
	if ((fmt & SurfaceTransformFlags::Rotate270) != SurfaceTransformFlags::None) { stream << " Rotate270"; }
	if ((fmt & SurfaceTransformFlags::Mirror) != SurfaceTransformFlags::None) { stream << " Mirror"; }
	if ((fmt & SurfaceTransformFlags::MirrorRotate90) != SurfaceTransformFlags::None) { stream << " MirrorRotate90"; }
	if ((fmt & SurfaceTransformFlags::MirrorRotate180) != SurfaceTransformFlags::None) { stream << " MirrorRotate180"; }
	if ((fmt & SurfaceTransformFlags::MirrorRotate270) != SurfaceTransformFlags::None) { stream << " MirrorRotate270"; }
	if ((fmt & SurfaceTransformFlags::Inherit) != SurfaceTransformFlags::None) { stream << " Inherit"; }
	return stream.str();
}

String getImageUsageDescription(ImageUsage fmt) {
	StringStream stream;
	if ((fmt & ImageUsage::TransferSrc) != ImageUsage::None) { stream << " TransferSrc"; }
	if ((fmt & ImageUsage::TransferDst) != ImageUsage::None) { stream << " TransferDst"; }
	if ((fmt & ImageUsage::Sampled) != ImageUsage::None) { stream << " Sampled"; }
	if ((fmt & ImageUsage::Storage) != ImageUsage::None) { stream << " Storage"; }
	if ((fmt & ImageUsage::ColorAttachment) != ImageUsage::None) { stream << " ColorAttachment"; }
	if ((fmt & ImageUsage::DepthStencilAttachment) != ImageUsage::None) { stream << " DepthStencilAttachment"; }
	if ((fmt & ImageUsage::TransientAttachment) != ImageUsage::None) { stream << " TransientAttachment"; }
	if ((fmt & ImageUsage::InputAttachment) != ImageUsage::None) { stream << " InputAttachment"; }
	return stream.str();
}

String BufferInfo::description() const {
	StringStream stream;

	stream << "BufferInfo: " << size << " bytes; Flags:";
	if (flags != BufferFlags::None) {
		stream << getBufferFlagsDescription(flags);
	} else {
		stream << " None";
	}
	stream << ";  Usage:";
	if (usage != BufferUsage::None) {
		stream << getBufferUsageDescription(usage);
	} else {
		stream << " None";
	}
	stream << ";";
	if (persistent) {
		stream << " Persistent;";
	}
	return stream.str();
}

bool ImageInfo::isCompatible(const ImageInfo &img) const {
	if (img.format == format && img.flags == flags && img.imageType == imageType && img.mipLevels == img.mipLevels
			&& img.arrayLayers == arrayLayers && img.samples == samples && img.tiling == tiling && img.usage == usage) {
		return true;
	}
	return true;
}

ImageViewInfo ImageInfo::getViewInfo(const ImageViewInfo &info) const {
	ImageViewInfo ret(info);
	if (ret.format == ImageFormat::Undefined) {
		ret.format = format;
	}
	if (ret.layerCount.get() == maxOf<uint32_t>()) {
		ret.layerCount = ArrayLayers(arrayLayers.get() - ret.baseArrayLayer.get());
	}
	return ret;
}

String ImageInfo::description() const {
	StringStream stream;
	stream << "ImageInfo: " << getImageFormatName(format) << " (" << getImageTypeName(imageType) << "); ";
	stream << extent.width << " x " << extent.height << " x " << extent.depth << "; Flags:";

	if (flags != ImageFlags::None) {
		stream << getImageFlagsDescription(flags);
	} else {
		stream << " None";
	}

	stream << "; MipLevels: " << mipLevels.get() << "; ArrayLayers: " << arrayLayers.get()
			<< "; Samples:" << getSampleCountDescription(samples) << "; Tiling: " << getImageTilingName(tiling) << "; Usage:";

	if (usage != ImageUsage::None) {
		stream << getImageUsageDescription(usage);
	} else {
		stream << " None";
	}
	stream << ";";
	return stream.str();
}

ImageData ImageData::make(Rc<ImageObject> &&obj) {
	ImageData ret;
	static_cast<gl::ImageInfo &>(ret) = obj->getInfo();
	ret.key = StringView(obj->getInfo().key);
	ret.image = move(obj);
	return ret;
}

void ImageViewInfo::setup(const renderqueue::ImageAttachmentDescriptor &desc) {
	bool allowSwizzle = (desc.getDescriptorType() != renderqueue::DescriptorType::InputAttachment
			&& desc.getDescriptorType() != renderqueue::DescriptorType::StorageImage);
	if (allowSwizzle) {
		for (auto &it : desc.getRefs()) {
			if ((it->getUsage() & renderqueue::AttachmentUsage::Input) != renderqueue::AttachmentUsage::None) {
				allowSwizzle = false;
				break;
			}
		}
	}

	auto &info = desc.getInfo();

	format = info.format;
	baseArrayLayer = BaseArrayLayer(0);
	layerCount = info.arrayLayers;
	switch (info.imageType) {
	case gl::ImageType::Image1D:
		type = gl::ImageViewType::ImageView1D;
		break;
	case gl::ImageType::Image2D:
		type = gl::ImageViewType::ImageView2D;
		break;
	case gl::ImageType::Image3D:
		type = gl::ImageViewType::ImageView3D;
		break;
	}

	setup(desc.getColorMode(), allowSwizzle);

	if (!allowSwizzle) {
		// input attachment cannot have swizzle mask

		if (r != gl::ComponentMapping::Identity) {
			r = gl::ComponentMapping::Identity;
			log::vtext("gl::ImageView", "Attachment descriptor '", desc.getName(),
					"' can not have non-identity ColorMode because it's used as input attachment");
		}

		if (g != gl::ComponentMapping::Identity) {
			g = gl::ComponentMapping::Identity;
			log::vtext("gl::ImageView", "Attachment descriptor '", desc.getName(),
					"' can not have non-identity ColorMode because it's used as input attachment");
		}

		if (b != gl::ComponentMapping::Identity) {
			b = gl::ComponentMapping::Identity;
			log::vtext("gl::ImageView", "Attachment descriptor '", desc.getName(),
					"' can not have non-identity ColorMode because it's used as input attachment");
		}

		if (a != gl::ComponentMapping::Identity) {
			a = gl::ComponentMapping::Identity;
			log::vtext("gl::ImageView", "Attachment descriptor '", desc.getName(),
					"' can not have non-identity ColorMode because it's used as input attachment");
		}
	}
}

void ImageViewInfo::setup(const ImageViewInfo &value) {
	*this = value;
}

void ImageViewInfo::setup(const ImageInfo &value) {
	format = value.format;
	baseArrayLayer = BaseArrayLayer(0);
	layerCount = value.arrayLayers;

	switch (value.imageType) {
	case gl::ImageType::Image1D:
		type = gl::ImageViewType::ImageView1D;
		break;
	case gl::ImageType::Image2D:
		type = gl::ImageViewType::ImageView2D;
		break;
	case gl::ImageType::Image3D:
		type = gl::ImageViewType::ImageView3D;
		break;
	}
}

void ImageViewInfo::setup(ColorMode value, bool allowSwizzle) {
	switch (value.getMode()) {
	case ColorMode::Solid: {
		if (!allowSwizzle) {
			r = gl::ComponentMapping::Identity;
			g = gl::ComponentMapping::Identity;
			b = gl::ComponentMapping::Identity;
			a = gl::ComponentMapping::Identity;
			return;
		}
		auto f = gl::getImagePixelFormat(format);
		switch (f) {
		case gl::PixelFormat::Unknown: break;
		case gl::PixelFormat::A:
			r = gl::ComponentMapping::One;
			g = gl::ComponentMapping::One;
			b = gl::ComponentMapping::One;
			a = gl::ComponentMapping::R;
			break;
		case gl::PixelFormat::IA:
			r = gl::ComponentMapping::R;
			g = gl::ComponentMapping::R;
			b = gl::ComponentMapping::R;
			a = gl::ComponentMapping::G;
			break;
		case gl::PixelFormat::RGB:
			r = gl::ComponentMapping::Identity;
			g = gl::ComponentMapping::Identity;
			b = gl::ComponentMapping::Identity;
			a = gl::ComponentMapping::One;
			break;
		case gl::PixelFormat::RGBA:
		case gl::PixelFormat::D:
		case gl::PixelFormat::DS:
		case gl::PixelFormat::S:
			r = gl::ComponentMapping::Identity;
			g = gl::ComponentMapping::Identity;
			b = gl::ComponentMapping::Identity;
			a = gl::ComponentMapping::Identity;
			break;
		}

		break;
	}
	case ColorMode::Custom:
		r = value.getR();
		g = value.getG();
		b = value.getB();
		a = value.getA();
		break;
	}
}

ColorMode ImageViewInfo::getColorMode() const {
	auto f = gl::getImagePixelFormat(format);
	switch (f) {
	case gl::PixelFormat::Unknown: return ColorMode(); break;
	case gl::PixelFormat::A:
		if (r == gl::ComponentMapping::One
				&& g == gl::ComponentMapping::One
				&& b == gl::ComponentMapping::One
				&& a == gl::ComponentMapping::R) {
			return ColorMode();
		}
		break;
	case gl::PixelFormat::IA:
		if (r == gl::ComponentMapping::R
				&& g == gl::ComponentMapping::R
				&& b == gl::ComponentMapping::R
				&& a == gl::ComponentMapping::G) {
			return ColorMode();
		}
		break;
	case gl::PixelFormat::RGB:
		if (r == gl::ComponentMapping::Identity
				&& g == gl::ComponentMapping::Identity
				&& b == gl::ComponentMapping::Identity
				&& a == gl::ComponentMapping::One) {
			return ColorMode();
		}
		break;
	case gl::PixelFormat::RGBA:
	case gl::PixelFormat::D:
	case gl::PixelFormat::DS:
	case gl::PixelFormat::S:
		if (r == gl::ComponentMapping::Identity
				&& g == gl::ComponentMapping::Identity
				&& b == gl::ComponentMapping::Identity
				&& a == gl::ComponentMapping::Identity) {
			return ColorMode();
		}
		break;
	}
	return ColorMode(r, g, b, a);
}

bool ImageViewInfo::isCompatible(const ImageInfo &info) const {
	// not perfect, multi-planar format not tracked, bun enough for now
	if (format != ImageFormat::Undefined && getFormatBlockSize(info.format) != getFormatBlockSize(format)) {
		return false;
	}

	// check type compatibility
	switch (type) {
	case gl::ImageViewType::ImageView1D:
		if (info.imageType != gl::ImageType::Image1D) {
			return false;
		}
		break;
	case gl::ImageViewType::ImageView1DArray:
		if (info.imageType != gl::ImageType::Image1D) {
			return false;
		}
		break;
	case gl::ImageViewType::ImageView2D:
		if (info.imageType != gl::ImageType::Image2D && info.imageType != gl::ImageType::Image3D) {
			return false;
		}
		break;
	case gl::ImageViewType::ImageView2DArray:
		if (info.imageType != gl::ImageType::Image2D && info.imageType != gl::ImageType::Image3D) {
			return false;
		}
		break;
	case gl::ImageViewType::ImageView3D:
		if (info.imageType != gl::ImageType::Image3D) {
			return false;
		}
		break;
	case gl::ImageViewType::ImageViewCube:
		if (info.imageType != gl::ImageType::Image2D) {
			return false;
		}
		break;
	case gl::ImageViewType::ImageViewCubeArray:
		if (info.imageType != gl::ImageType::Image2D) {
			return false;
		}
		break;
	}

	// check array size compatibility
	if (baseArrayLayer.get() >= info.arrayLayers.get()) {
		return false;
	}

	if (layerCount.get() != maxOf<uint32_t>() && baseArrayLayer.get() + layerCount.get() > info.arrayLayers.get()) {
		return false;
	}

	return true;
}

String ImageViewInfo::description() const {
	StringStream stream;
	stream << "ImageViewInfo: " << getImageFormatName(format) << " (" << getImageViewTypeName(type) << "); ";
	stream << "ArrayLayers: " << baseArrayLayer.get() << " (" << layerCount.get() << "); ";
	stream << "R -> " << getComponentMappingName(r) << "; ";
	stream << "G -> " << getComponentMappingName(g) << "; ";
	stream << "B -> " << getComponentMappingName(b) << "; ";
	stream << "A -> " << getComponentMappingName(a) << "; ";
	return stream.str();
}

size_t getFormatBlockSize(ImageFormat format) {
	switch (format) {
	case ImageFormat::Undefined: return 0; break;
	case ImageFormat::R4G4_UNORM_PACK8: return 1; break;
	case ImageFormat::R4G4B4A4_UNORM_PACK16: return 2; break;
	case ImageFormat::B4G4R4A4_UNORM_PACK16: return 2; break;
	case ImageFormat::R5G6B5_UNORM_PACK16: return 2; break;
	case ImageFormat::B5G6R5_UNORM_PACK16: return 2; break;
	case ImageFormat::R5G5B5A1_UNORM_PACK16: return 2; break;
	case ImageFormat::B5G5R5A1_UNORM_PACK16: return 2; break;
	case ImageFormat::A1R5G5B5_UNORM_PACK16: return 2; break;
	case ImageFormat::R8_UNORM: return 1; break;
	case ImageFormat::R8_SNORM: return 1; break;
	case ImageFormat::R8_USCALED: return 1; break;
	case ImageFormat::R8_SSCALED: return 1; break;
	case ImageFormat::R8_UINT: return 1; break;
	case ImageFormat::R8_SINT: return 1; break;
	case ImageFormat::R8_SRGB: return 1; break;
	case ImageFormat::R8G8_UNORM: return 2; break;
	case ImageFormat::R8G8_SNORM: return 2; break;
	case ImageFormat::R8G8_USCALED: return 2; break;
	case ImageFormat::R8G8_SSCALED: return 2; break;
	case ImageFormat::R8G8_UINT: return 2; break;
	case ImageFormat::R8G8_SINT: return 2; break;
	case ImageFormat::R8G8_SRGB: return 2; break;
	case ImageFormat::R8G8B8_UNORM: return 3; break;
	case ImageFormat::R8G8B8_SNORM: return 3; break;
	case ImageFormat::R8G8B8_USCALED: return 3; break;
	case ImageFormat::R8G8B8_SSCALED: return 3; break;
	case ImageFormat::R8G8B8_UINT: return 3; break;
	case ImageFormat::R8G8B8_SINT: return 3; break;
	case ImageFormat::R8G8B8_SRGB: return 3; break;
	case ImageFormat::B8G8R8_UNORM: return 3; break;
	case ImageFormat::B8G8R8_SNORM: return 3; break;
	case ImageFormat::B8G8R8_USCALED: return 3; break;
	case ImageFormat::B8G8R8_SSCALED: return 3; break;
	case ImageFormat::B8G8R8_UINT: return 3; break;
	case ImageFormat::B8G8R8_SINT: return 3; break;
	case ImageFormat::B8G8R8_SRGB: return 3; break;
	case ImageFormat::R8G8B8A8_UNORM: return 4; break;
	case ImageFormat::R8G8B8A8_SNORM: return 4; break;
	case ImageFormat::R8G8B8A8_USCALED: return 4; break;
	case ImageFormat::R8G8B8A8_SSCALED: return 4; break;
	case ImageFormat::R8G8B8A8_UINT: return 4; break;
	case ImageFormat::R8G8B8A8_SINT: return 4; break;
	case ImageFormat::R8G8B8A8_SRGB: return 4; break;
	case ImageFormat::B8G8R8A8_UNORM: return 4; break;
	case ImageFormat::B8G8R8A8_SNORM: return 4; break;
	case ImageFormat::B8G8R8A8_USCALED: return 4; break;
	case ImageFormat::B8G8R8A8_SSCALED: return 4; break;
	case ImageFormat::B8G8R8A8_UINT: return 4; break;
	case ImageFormat::B8G8R8A8_SINT: return 4; break;
	case ImageFormat::B8G8R8A8_SRGB: return 4; break;
	case ImageFormat::A8B8G8R8_UNORM_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_SNORM_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_USCALED_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_SSCALED_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_UINT_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_SINT_PACK32: return 4; break;
	case ImageFormat::A8B8G8R8_SRGB_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_UNORM_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_SNORM_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_USCALED_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_SSCALED_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_UINT_PACK32: return 4; break;
	case ImageFormat::A2R10G10B10_SINT_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_UNORM_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_SNORM_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_USCALED_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_SSCALED_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_UINT_PACK32: return 4; break;
	case ImageFormat::A2B10G10R10_SINT_PACK32: return 4; break;
	case ImageFormat::R16_UNORM: return 2; break;
	case ImageFormat::R16_SNORM: return 2; break;
	case ImageFormat::R16_USCALED: return 2; break;
	case ImageFormat::R16_SSCALED: return 2; break;
	case ImageFormat::R16_UINT: return 2; break;
	case ImageFormat::R16_SINT: return 2; break;
	case ImageFormat::R16_SFLOAT: return 2; break;
	case ImageFormat::R16G16_UNORM: return 4; break;
	case ImageFormat::R16G16_SNORM: return 4; break;
	case ImageFormat::R16G16_USCALED: return 4; break;
	case ImageFormat::R16G16_SSCALED: return 4; break;
	case ImageFormat::R16G16_UINT: return 4; break;
	case ImageFormat::R16G16_SINT: return 4; break;
	case ImageFormat::R16G16_SFLOAT: return 4; break;
	case ImageFormat::R16G16B16_UNORM: return 6; break;
	case ImageFormat::R16G16B16_SNORM: return 6; break;
	case ImageFormat::R16G16B16_USCALED: return 6; break;
	case ImageFormat::R16G16B16_SSCALED: return 6; break;
	case ImageFormat::R16G16B16_UINT: return 6; break;
	case ImageFormat::R16G16B16_SINT: return 6; break;
	case ImageFormat::R16G16B16_SFLOAT: return 6; break;
	case ImageFormat::R16G16B16A16_UNORM: return 8; break;
	case ImageFormat::R16G16B16A16_SNORM: return 8; break;
	case ImageFormat::R16G16B16A16_USCALED: return 8; break;
	case ImageFormat::R16G16B16A16_SSCALED: return 8; break;
	case ImageFormat::R16G16B16A16_UINT: return 8; break;
	case ImageFormat::R16G16B16A16_SINT: return 8; break;
	case ImageFormat::R16G16B16A16_SFLOAT: return 8; break;
	case ImageFormat::R32_UINT: return 4; break;
	case ImageFormat::R32_SINT: return 4; break;
	case ImageFormat::R32_SFLOAT: return 4; break;
	case ImageFormat::R32G32_UINT: return 8; break;
	case ImageFormat::R32G32_SINT: return 8; break;
	case ImageFormat::R32G32_SFLOAT: return 8; break;
	case ImageFormat::R32G32B32_UINT: return 12; break;
	case ImageFormat::R32G32B32_SINT: return 12; break;
	case ImageFormat::R32G32B32_SFLOAT: return 12; break;
	case ImageFormat::R32G32B32A32_UINT: return 16; break;
	case ImageFormat::R32G32B32A32_SINT: return 16; break;
	case ImageFormat::R32G32B32A32_SFLOAT: return 16; break;
	case ImageFormat::R64_UINT: return 8; break;
	case ImageFormat::R64_SINT: return 8; break;
	case ImageFormat::R64_SFLOAT: return 8; break;
	case ImageFormat::R64G64_UINT: return 16; break;
	case ImageFormat::R64G64_SINT: return 16; break;
	case ImageFormat::R64G64_SFLOAT: return 16; break;
	case ImageFormat::R64G64B64_UINT: return 24; break;
	case ImageFormat::R64G64B64_SINT: return 24; break;
	case ImageFormat::R64G64B64_SFLOAT: return 24; break;
	case ImageFormat::R64G64B64A64_UINT: return 32; break;
	case ImageFormat::R64G64B64A64_SINT: return 32; break;
	case ImageFormat::R64G64B64A64_SFLOAT: return 32; break;
	case ImageFormat::B10G11R11_UFLOAT_PACK32: return 4; break;
	case ImageFormat::E5B9G9R9_UFLOAT_PACK32: return 4; break;
	case ImageFormat::D16_UNORM: return 2; break;
	case ImageFormat::X8_D24_UNORM_PACK32: return 4; break;
	case ImageFormat::D32_SFLOAT: return 4; break;
	case ImageFormat::S8_UINT: return 1; break;
	case ImageFormat::D16_UNORM_S8_UINT: return 3; break;
	case ImageFormat::D24_UNORM_S8_UINT: return 4; break;
	case ImageFormat::D32_SFLOAT_S8_UINT: return 5; break;
	case ImageFormat::BC1_RGB_UNORM_BLOCK: return 8; break;
	case ImageFormat::BC1_RGB_SRGB_BLOCK: return 8; break;
	case ImageFormat::BC1_RGBA_UNORM_BLOCK: return 8; break;
	case ImageFormat::BC1_RGBA_SRGB_BLOCK: return 8; break;
	case ImageFormat::BC2_UNORM_BLOCK: return 16; break;
	case ImageFormat::BC2_SRGB_BLOCK: return 16; break;
	case ImageFormat::BC3_UNORM_BLOCK: return 16; break;
	case ImageFormat::BC3_SRGB_BLOCK: return 16; break;
	case ImageFormat::BC4_UNORM_BLOCK: return 8; break;
	case ImageFormat::BC4_SNORM_BLOCK: return 8; break;
	case ImageFormat::BC5_UNORM_BLOCK: return 16; break;
	case ImageFormat::BC5_SNORM_BLOCK: return 16; break;
	case ImageFormat::BC6H_UFLOAT_BLOCK: return 16; break;
	case ImageFormat::BC6H_SFLOAT_BLOCK: return 16; break;
	case ImageFormat::BC7_UNORM_BLOCK: return 16; break;
	case ImageFormat::BC7_SRGB_BLOCK: return 16; break;
	case ImageFormat::ETC2_R8G8B8_UNORM_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8_SRGB_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8A1_UNORM_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8A1_SRGB_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8A8_UNORM_BLOCK: return 8; break;
	case ImageFormat::ETC2_R8G8B8A8_SRGB_BLOCK: return 8; break;
	case ImageFormat::EAC_R11_UNORM_BLOCK: return 8; break;
	case ImageFormat::EAC_R11_SNORM_BLOCK: return 8; break;
	case ImageFormat::EAC_R11G11_UNORM_BLOCK: return 16; break;
	case ImageFormat::EAC_R11G11_SNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_4x4_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_4x4_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_5x4_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_5x4_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_5x5_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_5x5_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_6x5_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_6x5_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_6x6_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_6x6_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x5_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x5_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x6_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x6_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x8_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_8x8_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x5_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x5_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x6_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x6_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x8_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x8_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x10_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_10x10_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_12x10_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_12x10_SRGB_BLOCK: return 16; break;
	case ImageFormat::ASTC_12x12_UNORM_BLOCK: return 16; break;
	case ImageFormat::ASTC_12x12_SRGB_BLOCK: return 16; break;
	case ImageFormat::G8B8G8R8_422_UNORM: return 4; break;
	case ImageFormat::B8G8R8G8_422_UNORM: return 4; break;
	case ImageFormat::G8_B8_R8_3PLANE_420_UNORM: return 3; break;
	case ImageFormat::G8_B8R8_2PLANE_420_UNORM: return 3; break;
	case ImageFormat::G8_B8_R8_3PLANE_422_UNORM: return 3; break;
	case ImageFormat::G8_B8R8_2PLANE_422_UNORM: return 3; break;
	case ImageFormat::G8_B8_R8_3PLANE_444_UNORM: return 3; break;
	case ImageFormat::R10X6_UNORM_PACK16: return 2; break;
	case ImageFormat::R10X6G10X6_UNORM_2PACK16: return 4; break;
	case ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16: return 8; break;
	case ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return 8; break;
	case ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return 8; break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return 6; break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return 6; break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return 6; break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return 4; break;
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return 6; break;
	case ImageFormat::R12X4_UNORM_PACK16: return 2; break;
	case ImageFormat::R12X4G12X4_UNORM_2PACK16: return 4; break;
	case ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16: return 8; break;
	case ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return 8; break;
	case ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return 8; break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return 6; break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return 6; break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return 6; break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return 6; break;
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return 6; break;
	case ImageFormat::G16B16G16R16_422_UNORM: return 8; break;
	case ImageFormat::B16G16R16G16_422_UNORM: return 8; break;
	case ImageFormat::G16_B16_R16_3PLANE_420_UNORM: return 6; break;
	case ImageFormat::G16_B16R16_2PLANE_420_UNORM: return 6; break;
	case ImageFormat::G16_B16_R16_3PLANE_422_UNORM: return 6; break;
	case ImageFormat::G16_B16R16_2PLANE_422_UNORM: return 6; break;
	case ImageFormat::G16_B16_R16_3PLANE_444_UNORM: return 6; break;
	case ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG: return 8; break;
	case ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG: return 8; break;
	case ImageFormat::ASTC_4x4_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_5x4_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_5x5_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_6x5_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_6x6_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_8x5_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_8x6_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_8x8_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_10x5_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_10x6_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_10x8_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_10x10_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_12x10_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::ASTC_12x12_SFLOAT_BLOCK_EXT: return 8; break;
	case ImageFormat::G8_B8R8_2PLANE_444_UNORM_EXT: return 3; break;
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT: return 6; break;
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT: return 6; break;
	case ImageFormat::G16_B16R16_2PLANE_444_UNORM_EXT: return 6; break;
	case ImageFormat::A4R4G4B4_UNORM_PACK16_EXT: return 2; break;
	case ImageFormat::A4B4G4R4_UNORM_PACK16_EXT: return 2; break;
	}
	return 0;
}

PixelFormat getImagePixelFormat(ImageFormat format) {
	switch (format) {
	case ImageFormat::Undefined: return PixelFormat::Unknown; break;

	case ImageFormat::R8_UNORM:
	case ImageFormat::R8_SNORM:
	case ImageFormat::R8_USCALED:
	case ImageFormat::R8_SSCALED:
	case ImageFormat::R8_UINT:
	case ImageFormat::R8_SINT:
	case ImageFormat::R8_SRGB:
	case ImageFormat::R16_UNORM:
	case ImageFormat::R16_SNORM:
	case ImageFormat::R16_USCALED:
	case ImageFormat::R16_SSCALED:
	case ImageFormat::R16_UINT:
	case ImageFormat::R16_SINT:
	case ImageFormat::R16_SFLOAT:
	case ImageFormat::R32_UINT:
	case ImageFormat::R32_SINT:
	case ImageFormat::R32_SFLOAT:
	case ImageFormat::R64_UINT:
	case ImageFormat::R64_SINT:
	case ImageFormat::R64_SFLOAT:
	case ImageFormat::EAC_R11_UNORM_BLOCK:
	case ImageFormat::EAC_R11_SNORM_BLOCK:
	case ImageFormat::R10X6_UNORM_PACK16:
	case ImageFormat::R12X4_UNORM_PACK16:
		return PixelFormat::A;
		break;

	case ImageFormat::R4G4_UNORM_PACK8:
	case ImageFormat::R8G8_UNORM:
	case ImageFormat::R8G8_SNORM:
	case ImageFormat::R8G8_USCALED:
	case ImageFormat::R8G8_SSCALED:
	case ImageFormat::R8G8_UINT:
	case ImageFormat::R8G8_SINT:
	case ImageFormat::R8G8_SRGB:
	case ImageFormat::R16G16_UNORM:
	case ImageFormat::R16G16_SNORM:
	case ImageFormat::R16G16_USCALED:
	case ImageFormat::R16G16_SSCALED:
	case ImageFormat::R16G16_UINT:
	case ImageFormat::R16G16_SINT:
	case ImageFormat::R16G16_SFLOAT:
	case ImageFormat::R32G32_UINT:
	case ImageFormat::R32G32_SINT:
	case ImageFormat::R32G32_SFLOAT:
	case ImageFormat::R64G64_UINT:
	case ImageFormat::R64G64_SINT:
	case ImageFormat::R64G64_SFLOAT:
	case ImageFormat::EAC_R11G11_UNORM_BLOCK:
	case ImageFormat::EAC_R11G11_SNORM_BLOCK:
	case ImageFormat::R10X6G10X6_UNORM_2PACK16:
	case ImageFormat::R12X4G12X4_UNORM_2PACK16:
		return PixelFormat::IA;
		break;

	case ImageFormat::R4G4B4A4_UNORM_PACK16:
	case ImageFormat::B4G4R4A4_UNORM_PACK16:
	case ImageFormat::R5G5B5A1_UNORM_PACK16:
	case ImageFormat::B5G5R5A1_UNORM_PACK16:
	case ImageFormat::A1R5G5B5_UNORM_PACK16:
	case ImageFormat::R8G8B8A8_UNORM:
	case ImageFormat::R8G8B8A8_SNORM:
	case ImageFormat::R8G8B8A8_USCALED:
	case ImageFormat::R8G8B8A8_SSCALED:
	case ImageFormat::R8G8B8A8_UINT:
	case ImageFormat::R8G8B8A8_SINT:
	case ImageFormat::R8G8B8A8_SRGB:
	case ImageFormat::B8G8R8A8_UNORM:
	case ImageFormat::B8G8R8A8_SNORM:
	case ImageFormat::B8G8R8A8_USCALED:
	case ImageFormat::B8G8R8A8_SSCALED:
	case ImageFormat::B8G8R8A8_UINT:
	case ImageFormat::B8G8R8A8_SINT:
	case ImageFormat::B8G8R8A8_SRGB:
	case ImageFormat::A8B8G8R8_UNORM_PACK32:
	case ImageFormat::A8B8G8R8_SNORM_PACK32:
	case ImageFormat::A8B8G8R8_USCALED_PACK32:
	case ImageFormat::A8B8G8R8_SSCALED_PACK32:
	case ImageFormat::A8B8G8R8_UINT_PACK32:
	case ImageFormat::A8B8G8R8_SINT_PACK32:
	case ImageFormat::A8B8G8R8_SRGB_PACK32:
	case ImageFormat::A2R10G10B10_UNORM_PACK32:
	case ImageFormat::A2R10G10B10_SNORM_PACK32:
	case ImageFormat::A2R10G10B10_USCALED_PACK32:
	case ImageFormat::A2R10G10B10_SSCALED_PACK32:
	case ImageFormat::A2R10G10B10_UINT_PACK32:
	case ImageFormat::A2R10G10B10_SINT_PACK32:
	case ImageFormat::A2B10G10R10_UNORM_PACK32:
	case ImageFormat::A2B10G10R10_SNORM_PACK32:
	case ImageFormat::A2B10G10R10_USCALED_PACK32:
	case ImageFormat::A2B10G10R10_SSCALED_PACK32:
	case ImageFormat::A2B10G10R10_UINT_PACK32:
	case ImageFormat::A2B10G10R10_SINT_PACK32:
	case ImageFormat::R16G16B16A16_UNORM:
	case ImageFormat::R16G16B16A16_SNORM:
	case ImageFormat::R16G16B16A16_USCALED:
	case ImageFormat::R16G16B16A16_SSCALED:
	case ImageFormat::R16G16B16A16_UINT:
	case ImageFormat::R16G16B16A16_SINT:
	case ImageFormat::R16G16B16A16_SFLOAT:
	case ImageFormat::R32G32B32A32_UINT:
	case ImageFormat::R32G32B32A32_SINT:
	case ImageFormat::R32G32B32A32_SFLOAT:
	case ImageFormat::R64G64B64A64_UINT:
	case ImageFormat::R64G64B64A64_SINT:
	case ImageFormat::R64G64B64A64_SFLOAT:
	case ImageFormat::BC1_RGBA_UNORM_BLOCK:
	case ImageFormat::BC1_RGBA_SRGB_BLOCK:
	case ImageFormat::ETC2_R8G8B8A1_UNORM_BLOCK:
	case ImageFormat::ETC2_R8G8B8A1_SRGB_BLOCK:
	case ImageFormat::ETC2_R8G8B8A8_UNORM_BLOCK:
	case ImageFormat::ETC2_R8G8B8A8_SRGB_BLOCK:
	case ImageFormat::R10X6G10X6B10X6A10X6_UNORM_4PACK16:
	case ImageFormat::R12X4G12X4B12X4A12X4_UNORM_4PACK16:
	case ImageFormat::A4R4G4B4_UNORM_PACK16_EXT:
	case ImageFormat::A4B4G4R4_UNORM_PACK16_EXT:
		return PixelFormat::RGBA;
		break;

	case ImageFormat::R5G6B5_UNORM_PACK16:
	case ImageFormat::B5G6R5_UNORM_PACK16:
	case ImageFormat::R8G8B8_UNORM:
	case ImageFormat::R8G8B8_SNORM:
	case ImageFormat::R8G8B8_USCALED:
	case ImageFormat::R8G8B8_SSCALED:
	case ImageFormat::R8G8B8_UINT:
	case ImageFormat::R8G8B8_SINT:
	case ImageFormat::R8G8B8_SRGB:
	case ImageFormat::B8G8R8_UNORM:
	case ImageFormat::B8G8R8_SNORM:
	case ImageFormat::B8G8R8_USCALED:
	case ImageFormat::B8G8R8_SSCALED:
	case ImageFormat::B8G8R8_UINT:
	case ImageFormat::B8G8R8_SINT:
	case ImageFormat::B8G8R8_SRGB:
	case ImageFormat::R16G16B16_UNORM:
	case ImageFormat::R16G16B16_SNORM:
	case ImageFormat::R16G16B16_USCALED:
	case ImageFormat::R16G16B16_SSCALED:
	case ImageFormat::R16G16B16_UINT:
	case ImageFormat::R16G16B16_SINT:
	case ImageFormat::R16G16B16_SFLOAT:
	case ImageFormat::R32G32B32_UINT:
	case ImageFormat::R32G32B32_SINT:
	case ImageFormat::R32G32B32_SFLOAT:
	case ImageFormat::R64G64B64_UINT:
	case ImageFormat::R64G64B64_SINT:
	case ImageFormat::R64G64B64_SFLOAT:
	case ImageFormat::B10G11R11_UFLOAT_PACK32:
	case ImageFormat::G8B8G8R8_422_UNORM:
	case ImageFormat::B8G8R8G8_422_UNORM:
	case ImageFormat::BC1_RGB_UNORM_BLOCK:
	case ImageFormat::BC1_RGB_SRGB_BLOCK:
	case ImageFormat::ETC2_R8G8B8_UNORM_BLOCK:
	case ImageFormat::ETC2_R8G8B8_SRGB_BLOCK:
	case ImageFormat::G8_B8_R8_3PLANE_420_UNORM:
	case ImageFormat::G8_B8R8_2PLANE_420_UNORM:
	case ImageFormat::G8_B8_R8_3PLANE_422_UNORM:
	case ImageFormat::G8_B8R8_2PLANE_422_UNORM:
	case ImageFormat::G8_B8_R8_3PLANE_444_UNORM:
	case ImageFormat::G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
	case ImageFormat::B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
	case ImageFormat::G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
	case ImageFormat::G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
	case ImageFormat::B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
	case ImageFormat::G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
	case ImageFormat::G16B16G16R16_422_UNORM:
	case ImageFormat::B16G16R16G16_422_UNORM:
	case ImageFormat::G16_B16_R16_3PLANE_420_UNORM:
	case ImageFormat::G16_B16R16_2PLANE_420_UNORM:
	case ImageFormat::G16_B16_R16_3PLANE_422_UNORM:
	case ImageFormat::G16_B16R16_2PLANE_422_UNORM:
	case ImageFormat::G16_B16_R16_3PLANE_444_UNORM:
	case ImageFormat::G8_B8R8_2PLANE_444_UNORM_EXT:
	case ImageFormat::G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
	case ImageFormat::G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
	case ImageFormat::G16_B16R16_2PLANE_444_UNORM_EXT:
		return PixelFormat::RGB;
		break;

	case ImageFormat::D16_UNORM:
	case ImageFormat::D32_SFLOAT:
	case ImageFormat::X8_D24_UNORM_PACK32:
		return PixelFormat::D;
		break;

	case ImageFormat::S8_UINT:
		return PixelFormat::S;
		break;

	case ImageFormat::D16_UNORM_S8_UINT:
	case ImageFormat::D24_UNORM_S8_UINT:
	case ImageFormat::D32_SFLOAT_S8_UINT:
		return PixelFormat::DS;
		break;

	case ImageFormat::E5B9G9R9_UFLOAT_PACK32:
	case ImageFormat::BC2_UNORM_BLOCK:
	case ImageFormat::BC2_SRGB_BLOCK:
	case ImageFormat::BC3_UNORM_BLOCK:
	case ImageFormat::BC3_SRGB_BLOCK:
	case ImageFormat::BC4_UNORM_BLOCK:
	case ImageFormat::BC4_SNORM_BLOCK:
	case ImageFormat::BC5_UNORM_BLOCK:
	case ImageFormat::BC5_SNORM_BLOCK:
	case ImageFormat::BC6H_UFLOAT_BLOCK:
	case ImageFormat::BC6H_SFLOAT_BLOCK:
	case ImageFormat::BC7_UNORM_BLOCK:
	case ImageFormat::BC7_SRGB_BLOCK:
	case ImageFormat::ASTC_4x4_UNORM_BLOCK:
	case ImageFormat::ASTC_4x4_SRGB_BLOCK:
	case ImageFormat::ASTC_5x4_UNORM_BLOCK:
	case ImageFormat::ASTC_5x4_SRGB_BLOCK:
	case ImageFormat::ASTC_5x5_UNORM_BLOCK:
	case ImageFormat::ASTC_5x5_SRGB_BLOCK:
	case ImageFormat::ASTC_6x5_UNORM_BLOCK:
	case ImageFormat::ASTC_6x5_SRGB_BLOCK:
	case ImageFormat::ASTC_6x6_UNORM_BLOCK:
	case ImageFormat::ASTC_6x6_SRGB_BLOCK:
	case ImageFormat::ASTC_8x5_UNORM_BLOCK:
	case ImageFormat::ASTC_8x5_SRGB_BLOCK:
	case ImageFormat::ASTC_8x6_UNORM_BLOCK:
	case ImageFormat::ASTC_8x6_SRGB_BLOCK:
	case ImageFormat::ASTC_8x8_UNORM_BLOCK:
	case ImageFormat::ASTC_8x8_SRGB_BLOCK:
	case ImageFormat::ASTC_10x5_UNORM_BLOCK:
	case ImageFormat::ASTC_10x5_SRGB_BLOCK:
	case ImageFormat::ASTC_10x6_UNORM_BLOCK:
	case ImageFormat::ASTC_10x6_SRGB_BLOCK:
	case ImageFormat::ASTC_10x8_UNORM_BLOCK:
	case ImageFormat::ASTC_10x8_SRGB_BLOCK:
	case ImageFormat::ASTC_10x10_UNORM_BLOCK:
	case ImageFormat::ASTC_10x10_SRGB_BLOCK:
	case ImageFormat::ASTC_12x10_UNORM_BLOCK:
	case ImageFormat::ASTC_12x10_SRGB_BLOCK:
	case ImageFormat::ASTC_12x12_UNORM_BLOCK:
	case ImageFormat::ASTC_12x12_SRGB_BLOCK:
	case ImageFormat::PVRTC1_2BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC1_4BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC2_2BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC2_4BPP_UNORM_BLOCK_IMG:
	case ImageFormat::PVRTC1_2BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC1_4BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC2_2BPP_SRGB_BLOCK_IMG:
	case ImageFormat::PVRTC2_4BPP_SRGB_BLOCK_IMG:
	case ImageFormat::ASTC_4x4_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_5x4_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_5x5_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_6x5_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_6x6_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_8x5_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_8x6_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_8x8_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_10x5_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_10x6_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_10x8_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_10x10_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_12x10_SFLOAT_BLOCK_EXT:
	case ImageFormat::ASTC_12x12_SFLOAT_BLOCK_EXT:
		return PixelFormat::Unknown;
		break;
	}
	return PixelFormat::Unknown;
}

bool isStencilFormat(ImageFormat format) {
	switch (format) {
	case ImageFormat::S8_UINT:
	case ImageFormat::D16_UNORM_S8_UINT:
	case ImageFormat::D24_UNORM_S8_UINT:
	case ImageFormat::D32_SFLOAT_S8_UINT:
		return true;
		break;
	default:
		break;
	}
	return false;
}

bool isDepthFormat(ImageFormat format) {
	switch (format) {
	case ImageFormat::D16_UNORM:
	case ImageFormat::D32_SFLOAT:
	case ImageFormat::D16_UNORM_S8_UINT:
	case ImageFormat::D24_UNORM_S8_UINT:
	case ImageFormat::D32_SFLOAT_S8_UINT:
	case ImageFormat::X8_D24_UNORM_PACK32:
		return true;
		break;
	default:
		break;
	}
	return false;
}

String SwapchainConfig::description() const {
	StringStream stream;
	stream << "\nSurfaceInfo:\n";
	stream << "\tPresentMode: " << getPresentModeName(presentMode);
	if (presentModeFast != PresentMode::Unsupported) {
		stream << " (" << getPresentModeName(presentModeFast) << ")";
	}
	stream << "\n";
	stream << "\tSurface format: (" << getImageFormatName(imageFormat) << ":" << getColorSpaceName(colorSpace) << ")\n";
	stream << "\tTransform:" << getSurfaceTransformFlagsDescription(transform) << "\n";
	stream << "\tAlpha:" << getCompositeAlphaFlagsDescription(alpha) << "\n";
	stream << "\tImage count: " << imageCount << "\n";
	stream << "\tExtent: " << extent.width << "x" << extent.height << "\n";
	return stream.str();
}

bool SurfaceInfo::isSupported(const SwapchainConfig &cfg) const {
	if (std::find(presentModes.begin(), presentModes.end(), cfg.presentMode) == presentModes.end()) {
		log::vtext("Vk-Error", "SurfaceInfo: presentMode is not supported");
		return false;
	}

	if (cfg.presentModeFast != PresentMode::Unsupported && std::find(presentModes.begin(), presentModes.end(),
			cfg.presentModeFast) == presentModes.end()) {
		log::vtext("Vk-Error", "SurfaceInfo: presentModeFast is not supported");
		return false;
	}

	if (std::find(formats.begin(), formats.end(), pair(cfg.imageFormat, cfg.colorSpace)) == formats.end()) {
		log::vtext("Vk-Error", "SurfaceInfo: imageFormat or colorSpace is not supported");
		return false;
	}

	if ((supportedCompositeAlpha & cfg.alpha) == CompositeAlphaFlags::None) {
		log::vtext("Vk-Error", "SurfaceInfo: alpha is not supported");
		return false;
	}

	if ((supportedTransforms & cfg.transform) == SurfaceTransformFlags::None) {
		log::vtext("Vk-Error", "SurfaceInfo: transform is not supported");
		return false;
	}

	if (cfg.imageCount < minImageCount || (maxImageCount != 0 && cfg.imageCount > maxImageCount)) {
		log::vtext("Vk-Error", "SurfaceInfo: imageCount is not supported");
		return false;
	}

	if (cfg.extent.width < minImageExtent.width || cfg.extent.width > maxImageExtent.width
			|| cfg.extent.height < minImageExtent.height || cfg.extent.height > maxImageExtent.height) {
		log::vtext("Vk-Error", "SurfaceInfo: extent is not supported");
		return false;
	}

	if (cfg.transfer && (supportedUsageFlags & ImageUsage::TransferDst) == ImageUsage::None) {
		log::vtext("Vk-Error", "SurfaceInfo: supportedUsageFlags is not supported");
		return false;
	}

	return true;
}

String SurfaceInfo::description() const {
	StringStream stream;
	stream << "\nSurfaceInfo:\n";
	stream << "\tImageCount: " << minImageCount << "-" << maxImageCount << "\n";
	stream << "\tExtent: " << currentExtent.width << "x" << currentExtent.height
			<< " (" << minImageExtent.width << "x" << minImageExtent.height
			<< " - " << maxImageExtent.width << "x" << maxImageExtent.height << ")\n";
	stream << "\tMax Layers: " << maxImageArrayLayers << "\n";

	stream << "\tSupported transforms:" << getSurfaceTransformFlagsDescription(supportedTransforms) << "\n";
	stream << "\tCurrent transforms:" << getSurfaceTransformFlagsDescription(currentTransform) << "\n";
	stream << "\tSupported Alpha:" << getCompositeAlphaFlagsDescription(supportedCompositeAlpha) << "\n";
	stream << "\tSupported Usage:" << getImageUsageDescription(supportedUsageFlags) << "\n";

	stream << "\tSurface format:";
	for (auto it : formats) {
		stream << " (" << getImageFormatName(it.first) << ":" << getColorSpaceName(it.second) << ")";
	}
	stream << "\n";

	stream << "\tPresent modes:";
	for (auto &it : presentModes) {
		stream << " " << getPresentModeName(it);
	}
	stream << "\n";
	return stream.str();
}

std::ostream & operator<<(std::ostream &stream, const ImageInfoData &value) {
	stream << "ImageInfoData: " << value.extent;
	return stream;
}

}
