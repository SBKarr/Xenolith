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

#ifndef XENOLITH_GL_COMMON_XLGLRESOURCE_H_
#define XENOLITH_GL_COMMON_XLGLRESOURCE_H_

#include "XLGl.h"
#include "XLEventHeader.h"

namespace stappler::xenolith::gl {

class Resource : public NamedRef {
public:
	class Builder;

	Resource();
	virtual ~Resource();

	virtual bool init(Builder &&);

	void setInUse(bool);
	bool isInUse() const { return _inUse; }

	// active resource will receive updates from GL if compilation/transfer was requested
	// non-active resource will drop any received data
	void setActive(bool);
	bool isActive() const { return _active; }

	virtual StringView getName() const override;
	memory::pool_t *getPool() const;

	const HashTable<PipelineData *> &getPipelines() const;
	const HashTable<BufferData *> &getBuffers() const;
	const HashTable<ImageData *> &getImages() const;

	const PipelineData *getPipeline(StringView) const;
	const BufferData *getBuffer(StringView) const;
	const ImageData *getImage(StringView) const;

protected:
	friend class ResourceCache;

	struct ResourceData;

	bool _inUse = false; // set once, you should not modify resource after
	bool _active = false;
	ResourceData *_data = nullptr;
};

class Resource::Builder final {
public:
	Builder(StringView);
	~Builder();

	/* Buffer setup, arguments parsed by type without ordering
	 *
	 * BufferFlags - buffer creation flags, or'ed with existed values. Default: BufferFlags::None
	 * BufferUsage - buffer usage flags, or'ed with existed values. BufferUsage::TransferDst should always be enabled
	 * uint64_t - interpreted as buffer size. Default: data.size() or size of file content. Should be >= default
	 *
	 * example: resource->addBuffer("MyBuffer", data, BufferUsage::StorageBuffer, BufferFlags::Protected,
	 * 		BufferUsage::ShaderDeviceAddress);
	 * same: resource->addBuffer("MyBuffer", data, BufferFlags::Protected,
	 * 		BufferUsage::StorageBuffer | BufferUsage::ShaderDeviceAddress);
	 */

	template <typename ... Args>
	void addBufferByRef(StringView key, BytesView data, Args && ... args) {
		auto &b = doAddBufferByRef(key, data);
		setupBuffer(b, std::forward<Args>(args)...);
	}

	template <typename ... Args>
	void addBuffer(StringView key, FilePath data, Args && ... args) {
		auto &b = doAddBuffer(key, data);
		setupBuffer(b, std::forward<Args>(args)...);
	}

	template <typename ... Args>
	void addBuffer(StringView key, BytesView data, Args && ... args) {
		auto &b = doAddBuffer(key, data);
		setupBuffer(b, std::forward<Args>(args)...);
	}

	template <typename ... Args>
	void addBuffer(StringView key, size_t size, const memory::function<void(const BufferData::DataCallback &)> &cb, Args && ... args) {
		auto &b = doAddBuffer(key, size, cb);
		setupBuffer(b, std::forward<Args>(args)...);
	}


	/* Image setup, arguments parsed by type without ordering
	 *
	 * Extent1(N) - 1-dimensional image size, size of image must be defined as Extent1, Extent2 or Extent2
	 * Extent3 - 3-dimensional size of image, size of image must be defined as Extent1, Extent2 or Extent2
	 * Extent2 - 2-dimensional size of image (so, depth is set to 1), size of image must be defined as Extent1, Extent2 or Extent2
	 * ImageFlags - image creation flags, or'ed with existed values. Default: ImageFlags::None
	 * ImageType - 1D, 2D, 3D. Default: ImageType::Image2D
	 * MipLevels(N) - number of mipLevels. Default: 1
	 * ArrayLayers(N) - number of images in image array. Default: 1
	 * SampleCount - sample count enum for multisampling. Default: SampleCount::X1 (no multisampling)
	 * ImageTiling - image positioning in device memory, ImageTiling::Linear can be read and written by host,
	 * 		ImageTiling::Optimal is fastest for read|sample by device. Default: ImageTiling::Optimal
	 * ImageUsage - image usage flags, or'ed with existed values. ImageUsage::TransferDst should always be enabled
	 *
	 * example: resource->addImage("MyImage", data, ImageFormat::B8G8R8A8_UNORM,
	 * 		ImageUsage::Sampled,
	 * 		Extent2(768, 1024), // using 2d extent for 2d image
	 * 		ImageFlags::Protected,
	 * 		ImageType::2D, // can be omitted for 2d
	 * 		MipLevels(1), // can be omitted if there is no extra miplevels
	 * 		ArrayLayers(1), // can be omitted if image is not an array
	 * 		SampleCount::X1, // can be omitted if no multisampling required
	 * 		ImageTiling::Linear,
	 * 		);
	 * same: resource->addBuffer("MyBuffer", data, BufferFlags::Protected,
	 * 		BufferUsage::StorageBuffer | BufferUsage::ShaderDeviceAddress);
	 */

	template <typename ... Args>
	void addImageByRef(StringView key, BytesView data, ImageInfo &&img) {
		doAddImageByRef(key, data, move(img));
	}

	template <typename ... Args>
	void addImage(StringView key, FilePath data, ImageInfo &&img) {
		doAddImage(key, data, move(img));
	}

	template <typename ... Args>
	void addImage(StringView key, BytesView data, ImageInfo &&img) {
		doAddImage(key, data, move(img));
	}

	template <typename ... Args>
	void addImage(StringView key, const memory::function<void(const BufferData::DataCallback &)> &cb, ImageInfo &&img) {
		doAddImage(key, cb, move(img));
	}

protected:
	BufferData *doAddBuffer(StringView, BytesView);
	BufferData *doAddBuffer(StringView, FilePath);
	BufferData *doAddBufferByRef(StringView, BytesView);
	BufferData *doAddBuffer(StringView, size_t, const memory::function<void(const BufferData::DataCallback &)> &cb);

	ImageData *doAddImage(StringView, BytesView, ImageInfo &&img);
	ImageData *doAddImage(StringView, FilePath, ImageInfo &&img);
	ImageData *doAddImageByRef(StringView, BytesView, ImageInfo &&img);
	ImageData *doAddImage(StringView, const memory::function<void(const BufferData::DataCallback &)> &cb, ImageInfo &&img);

	void setBufferOption(BufferData &b, BufferFlags);
	void setBufferOption(BufferData &b, BufferUsage);
	void setBufferOption(BufferData &b, uint64_t);

	template <typename T>
	void setupBuffer(BufferData &b, T && t) {
		setBufferOption(b, std::forward<T>(t));
	}

	template <typename T, typename ... Args>
	void setupBuffer(BufferData &b, T && t, Args && ... args) {
		setupBuffer(b, std::forward<T>(t));
		setupBuffer(b, std::forward<Args>(args)...);
	}

	friend class Resource;

	ResourceData *_data = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLRESOURCE_H_ */
