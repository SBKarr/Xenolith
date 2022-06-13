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

#ifndef XENOLITH_GL_COMMON_XLGL_H_
#define XENOLITH_GL_COMMON_XLGL_H_

#include "XLHashTable.h"
#include "XLPlatform.h"
#include "XLRenderQueueEnum.h"
#include "SPBitmap.h"
#include "SPThread.h"
#include "SPThreadTaskQueue.h"
#include "SPEventTaskQueue.h"
#include <optional>

namespace stappler::xenolith::renderqueue {

class Resource;
class ImageAttachmentDescriptor;

}

namespace stappler::xenolith::gl {

using TaskQueue = thread::TaskQueue;
// using TaskQueue = thread::EventTaskQueue;

class Instance;
class Loop;
class Device;
class Shader;
class Pipeline;
class Framebuffer;
class ImageObject;
class ImageAtlas;
class ImageView;
class BufferObject;
class CommandList;
class Material;
class MaterialAttachment;
class DynamicImage;
class Semaphore;

using MaterialId = uint32_t;

using MipLevels = ValueWrapper<uint32_t, class MipLevelFlag>;
using ArrayLayers = ValueWrapper<uint32_t, class ArrayLayersFlag>;
using Extent1 = ValueWrapper<uint32_t, class Extent1Flag>;
using BaseArrayLayer = ValueWrapper<uint32_t, class BaseArrayLayerFlag>;

using RenderPassType = renderqueue::PassType;
using Resource = renderqueue::Resource;
using AttachmentInputData = renderqueue::AttachmentInputData;
using ProgramStage = renderqueue::ProgramStage;

enum class ObjectType {
	Unknown,
	Buffer,
	BufferView,
	CommandPool,
	DescriptorPool,
	DescriptorSetLayout,
	Event,
	Fence,
	Framebuffer,
	Image,
	ImageView,
	Pipeline,
	PipelineCache,
	PipelineLayout,
	QueryPool,
	RenderPass,
	Sampler,
	Semaphore,
	ShaderModule,
	DeviceMemory,
	Swapchain
};

enum class PixelFormat {
	Unknown,
	A, // single-channel color
	IA, // dual-channel color
	RGB,
	RGBA,
	D, // depth
	DS, // depth-stencil
	S // stencil
};

struct SamplerInfo {
	Filter magFilter = Filter::Nearest;
	Filter minFilter = Filter::Nearest;
	SamplerMipmapMode mipmapMode = SamplerMipmapMode::Nearest;
	SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
	SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
	SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;
	float mipLodBias = 0.0f;
	bool anisotropyEnable = false;
	float maxAnisotropy = 0.0f;
	bool compareEnable = false;
	CompareOp compareOp = CompareOp::Never;
	float minLod = 0.0;
	float maxLod = 0.0;

	bool operator==(const SamplerInfo &) const = default;
	bool operator!=(const SamplerInfo &) const = default;
	auto operator<=>(const SamplerInfo &) const = default;
};

using ForceBufferFlags = ValueWrapper<BufferFlags, class ForceBufferFlagsFlag>;
using ForceBufferUsage = ValueWrapper<BufferUsage, class ForceBufferUsageFlag>;
using BufferPersistent = ValueWrapper<bool, class BufferPersistentFlag>;

struct BufferInfo : NamedMem {
	BufferFlags flags = BufferFlags::None;
	BufferUsage usage = BufferUsage::TransferDst;

	// on which type of RenderPass this buffer will be used (there is no universal usage, so, think carefully)
	RenderPassType type = RenderPassType::Graphics;
	uint64_t size = 0;
	bool persistent = true;

	BufferInfo() = default;

	template<typename ... Args>
	BufferInfo(Args && ... args) {
		define(std::forward<Args>(args)...);
	}

	void setup(BufferFlags value) { flags |= value; }
	void setup(ForceBufferFlags value) { flags = value.get(); }
	void setup(BufferUsage value) { usage |= value; }
	void setup(ForceBufferUsage value) { usage = value.get(); }
	void setup(uint64_t value) { size = value; }
	void setup(BufferPersistent value) { persistent = value.get(); }
	void setup(RenderPassType value) { type = value; }

	template <typename T>
	void define(T && t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	void define(T && t, Args && ... args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	String description() const;
};

struct BufferData : BufferInfo {
	using DataCallback = memory::callback<void(BytesView)>;

	BytesView data;
	memory::function<void(const DataCallback &)> callback = nullptr;
	Rc<BufferObject> buffer; // GL implementation-dependent object
	const Resource *resource = nullptr; // owning resource;
};


using ForceImageFlags = ValueWrapper<ImageFlags, class ForceImageFlagsFlag>;
using ForceImageUsage = ValueWrapper<ImageUsage, class ForceImageUsageFlag>;

struct ImageViewInfo;

struct ImageInfoData {
	ImageFormat format = ImageFormat::Undefined;
	ImageFlags flags = ImageFlags::None;
	ImageType imageType = ImageType::Image2D;
	Extent3 extent = Extent3(1, 1, 1);
	MipLevels mipLevels = MipLevels(1);
	ArrayLayers arrayLayers = ArrayLayers(1);
	SampleCount samples = SampleCount::X1;
	ImageTiling tiling = ImageTiling::Optimal;
	ImageUsage usage = ImageUsage::TransferDst;

	// on which type of RenderPass this image will be used (there is no universal usage, so, think carefully)
	RenderPassType type = RenderPassType::Graphics;
	ImageHints hints = ImageHints::None;

	bool operator==(const ImageInfoData &) const = default;
	bool operator!=(const ImageInfoData &) const = default;
	auto operator<=>(const ImageInfoData &) const = default;
};

struct ImageInfo : NamedMem, ImageInfoData {
	ImageInfo() = default;

	template<typename ... Args>
	ImageInfo(Args && ... args) {
		define(std::forward<Args>(args)...);
	}

	void setup(Extent1 value) { extent = Extent3(value.get(), 1, 1); }
	void setup(Extent2 value) { extent = Extent3(value.width, value.height, 1); }
	void setup(Extent3 value) { extent = value; }
	void setup(ImageFlags value) { flags |= value; }
	void setup(ForceImageFlags value) { flags = value.get(); }
	void setup(ImageType value) { imageType = value; }
	void setup(MipLevels value) { mipLevels = value; }
	void setup(ArrayLayers value) { arrayLayers = value; }
	void setup(SampleCount value) { samples = value; }
	void setup(ImageTiling value) { tiling = value; }
	void setup(ImageUsage value) { usage |= value; }
	void setup(ForceImageUsage value) { usage = value.get(); }
	void setup(ImageFormat value) { format = value; }
	void setup(RenderPassType value) { type = value; }
	void setup(ImageHints value) { hints |= value; }
	void setup(StringView value) { key = value; }

	template <typename T>
	void define(T && t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	void define(T && t, Args && ... args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	bool isCompatible(const ImageInfo &) const;

	ImageViewInfo getViewInfo(const ImageViewInfo &info) const;

	String description() const;
};

struct ImageData : ImageInfo {
	using DataCallback = memory::callback<void(BytesView)>;

	static ImageData make(Rc<ImageObject> &&);

	BytesView data;
	memory::function<void(const DataCallback &)> memCallback = nullptr;
	Function<void(const DataCallback &)> stdCallback = nullptr;
	Rc<ImageObject> image; // GL implementation-dependent object
	Rc<ImageAtlas> atlas;
	const Resource *resource = nullptr; // owning resource;
};


using ComponentMappingR = ValueWrapper<ComponentMapping, class ComponentMappingRFlag>;
using ComponentMappingG = ValueWrapper<ComponentMapping, class ComponentMappingGFlag>;
using ComponentMappingB = ValueWrapper<ComponentMapping, class ComponentMappingBFlag>;
using ComponentMappingA = ValueWrapper<ComponentMapping, class ComponentMappingAFlag>;

struct ImageViewInfo {
	ImageFormat format = ImageFormat::Undefined; // inherited from Image if undefined
	ImageViewType type = ImageViewType::ImageView2D;
	ComponentMapping r = ComponentMapping::Identity;
	ComponentMapping g = ComponentMapping::Identity;
	ComponentMapping b = ComponentMapping::Identity;
	ComponentMapping a = ComponentMapping::Identity;
	BaseArrayLayer baseArrayLayer = BaseArrayLayer(0);
	ArrayLayers layerCount = ArrayLayers(maxOf<uint32_t>());

	ImageViewInfo() = default;
	ImageViewInfo(const ImageViewInfo &) = default;
	ImageViewInfo(ImageViewInfo &&) = default;
	ImageViewInfo &operator=(const ImageViewInfo &) = default;
	ImageViewInfo &operator=(ImageViewInfo &&) = default;

	template<typename ... Args>
	ImageViewInfo(Args && ... args) {
		define(std::forward<Args>(args)...);
	}

	void setup(const renderqueue::ImageAttachmentDescriptor &);
	void setup(const ImageViewInfo &);
	void setup(const ImageInfo &);
	void setup(ImageViewType value) { type = value; }
	void setup(ImageFormat value) { format = value; }
	void setup(ArrayLayers value) { layerCount = value; }
	void setup(BaseArrayLayer value) { baseArrayLayer = value; }
	void setup(ComponentMappingR value) { r = value.get(); }
	void setup(ComponentMappingG value) { g = value.get(); }
	void setup(ComponentMappingB value) { b = value.get(); }
	void setup(ComponentMappingA value) { a = value.get(); }
	void setup(ColorMode value);

	ColorMode getColorMode() const;

	template <typename T>
	void define(T && t) {
		setup(std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	void define(T && t, Args && ... args) {
		define(std::forward<T>(t));
		define(std::forward<Args>(args)...);
	}

	bool isCompatible(const ImageInfo &) const;
	String description() const;

	bool operator==(const ImageViewInfo &) const = default;
	bool operator!=(const ImageViewInfo &) const = default;
	auto operator<=>(const ImageViewInfo &) const = default;
};

// Designed to use with SSBO and std430
struct alignas(16) Vertex_V4F_V4F_T2F2U {
	Vec4 pos;
	Vec4 color;
	Vec2 tex;
	uint32_t material;
	uint32_t object;
};

struct Triangle_V3F_C4F_T2F {
	Vertex_V4F_V4F_T2F2U a;
	Vertex_V4F_V4F_T2F2U b;
	Vertex_V4F_V4F_T2F2U c;
};

struct Quad_V3F_C4F_T2F {
	Vertex_V4F_V4F_T2F2U tl;
	Vertex_V4F_V4F_T2F2U bl;
	Vertex_V4F_V4F_T2F2U tr;
	Vertex_V4F_V4F_T2F2U br;
};

struct VertexSpan {
	MaterialId material;
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t firstIndex;
};

struct VertexData : public AttachmentInputData {
	Vector<Vertex_V4F_V4F_T2F2U> data;
	Vector<uint32_t> indexes;
};

struct RenderFontInput : public AttachmentInputData {
	using FontRequest = Pair<Rc<font::FontFaceObject>, Vector<char16_t>>;

	Rc<DynamicImage> image;
	Vector<FontRequest> requests;
	Function<void(const ImageInfo &, BytesView)> output;
};

struct SwapchainConfig {
	PresentMode presentMode = PresentMode::Mailbox;
	PresentMode presentModeFast = PresentMode::Unsupported;
	ImageFormat imageFormat = platform::graphic::getCommonFormat();
	ColorSpace colorSpace = ColorSpace::SRGB_NONLINEAR_KHR;
	CompositeAlphaFlags alpha = CompositeAlphaFlags::Opaque;
	SurfaceTransformFlags transform = SurfaceTransformFlags::Identity;
	uint32_t imageCount = 3;
	Extent2 extent;
	bool clipped = false;
	bool transfer = true;

	String description() const;
};

struct SurfaceInfo {
	uint32_t minImageCount;
	uint32_t maxImageCount;
	Extent2 currentExtent;
	Extent2 minImageExtent;
	Extent2 maxImageExtent;
	uint32_t maxImageArrayLayers;
	CompositeAlphaFlags supportedCompositeAlpha;
	SurfaceTransformFlags supportedTransforms;
	SurfaceTransformFlags currentTransform;
	ImageUsage supportedUsageFlags;
	Vector<Pair<ImageFormat, ColorSpace>> formats;
	Vector<PresentMode> presentModes;

	bool isSupported(const SwapchainConfig &) const;

	String description() const;
};

struct ViewInfo {
	String name;
	URect rect;
	uint64_t frameInterval = 0; // in microseconds ( 1'000'000 / 60 for 60 fps)
	Function<SwapchainConfig (const SurfaceInfo &)> config;

	Function<void(const Rc<Director> &)> onCreated;
	Function<void()> onClosed;
};

String getBufferFlagsDescription(BufferFlags fmt);
String getBufferUsageDescription(BufferUsage fmt);
String getImageFlagsDescription(ImageFlags fmt);
String getSampleCountDescription(SampleCount fmt);
StringView getImageTypeName(ImageType type);
StringView getImageViewTypeName(ImageViewType type);
StringView getImageFormatName(ImageFormat fmt);
StringView getImageTilingName(ImageTiling type);
StringView getComponentMappingName(ComponentMapping);
StringView getPresentModeName(PresentMode);
StringView getColorSpaceName(ColorSpace);
String getCompositeAlphaFlagsDescription(CompositeAlphaFlags);
String getSurfaceTransformFlagsDescription(SurfaceTransformFlags);
String getImageUsageDescription(ImageUsage fmt);
size_t getFormatBlockSize(ImageFormat format);
PixelFormat getImagePixelFormat(ImageFormat format);
bool isStencilFormat(ImageFormat format);
bool isDepthFormat(ImageFormat format);

inline std::ostream & operator<<(std::ostream &stream, const Vertex_V4F_V4F_T2F2U &value) {
	stream << "Pos: " << value.pos << "; Color:" << value.color << "; Tex:" << value.tex << "; Mat:" << value.material << "," << value.object << ";";
	return stream;
}

}

#endif /* XENOLITH_GL_COMMON_XLGL_H_ */
