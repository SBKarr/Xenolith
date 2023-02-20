/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_GL_VK_RENDERER_XLVKSHADOWRENDERPASS_H_
#define XENOLITH_GL_VK_RENDERER_XLVKSHADOWRENDERPASS_H_

#include "XLVkAttachment.h"
#include "XLVkQueuePass.h"

namespace stappler::xenolith::vk {

class ShadowLightDataAttachment : public BufferAttachment {
public:
	using LightData = gl::glsl::ShadowData;

	virtual ~ShadowLightDataAttachment();

	virtual bool init(StringView);

	virtual bool validateInput(const Rc<gl::AttachmentInputData> &) const override;

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

// this attachment should provide vertex & index buffers
class ShadowVertexAttachment : public BufferAttachment {
public:
	virtual ~ShadowVertexAttachment();

	virtual bool init(StringView);

	virtual bool validateInput(const Rc<gl::AttachmentInputData> &) const override;

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

// this attachment should provide vertex & index buffers
class ShadowTrianglesAttachment : public BufferAttachment {
public:
	virtual ~ShadowTrianglesAttachment();

	virtual bool init(StringView);

protected:
	using BufferAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class ShadowImageArrayAttachment : public ImageAttachment {
public:
	virtual ~ShadowImageArrayAttachment();

	virtual bool init(StringView, Extent2 extent);

	virtual gl::ImageInfo getAttachmentInfo(const AttachmentHandle *, Extent3 e) const override;

protected:
	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class ShadowSdfImageAttachment : public ImageAttachment {
public:
	virtual ~ShadowSdfImageAttachment();

	virtual bool init(StringView, Extent2 extent);

	virtual gl::ImageInfo getAttachmentInfo(const AttachmentHandle *, Extent3 e) const override;

protected:
	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class ShadowPass : public QueuePass {
public:
	using AttachmentHandle = renderqueue::AttachmentHandle;

	static constexpr StringView SdfTrianglesComp = "SdfTrianglesComp";
	static constexpr StringView SdfCirclesComp = "SdfCirclesComp";
	static constexpr StringView SdfImageComp = "SdfImageComp";

	static bool makeDefaultRenderQueue(renderqueue::Queue::Builder &, Extent2 extent);

	virtual ~ShadowPass() { }

	virtual bool init(StringView, RenderOrdering);

	const ShadowLightDataAttachment *getLights() const { return _lights; }
	const ShadowVertexAttachment *getVertexes() const { return _vertexes; }
	const ShadowTrianglesAttachment *getTriangles() const { return _triangles; }
	const ShadowImageArrayAttachment *getArray() const { return _array; }

	virtual Rc<PassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	virtual void prepare(gl::Device &) override;

	const ShadowLightDataAttachment *_lights = nullptr;
	const ShadowVertexAttachment *_vertexes = nullptr;
	const ShadowTrianglesAttachment *_triangles = nullptr;
	const ShadowImageArrayAttachment *_array = nullptr;
};

class ShadowLightDataAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ShadowLightDataAttachmentHandle();

	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &) override;

	void allocateBuffer(DeviceFrameHandle *, uint32_t trianglesCount, float value, uint32_t gridCells, Extent2 extent);

	float getBoxOffset(float value) const;

	uint32_t getLightsCount() const;
	uint32_t getObjectsCount() const;

	const gl::glsl::ShadowData &getShadowData() const { return _shadowData; }
	const Rc<DeviceBuffer> &getBuffer() const { return _data; }

protected:
	Rc<DeviceBuffer> _data;
	Rc<gl::ShadowLightInput> _input;
	gl::glsl::ShadowData _shadowData;
};

class ShadowVertexAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ShadowVertexAttachmentHandle();

	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &) override;

	bool empty() const;

	uint32_t getTrianglesCount() const { return _trianglesCount; }
	uint32_t getCirclesCount() const { return _circlesCount; }
	float getMaxValue() const { return _maxValue; }

protected:
	virtual bool loadVertexes(FrameHandle &, const Rc<gl::CommandList> &);

	Rc<DeviceBuffer> _indexes;
	Rc<DeviceBuffer> _vertexes;
	Rc<DeviceBuffer> _transforms;
	Rc<DeviceBuffer> _circles;
	uint32_t _trianglesCount = 0;
	uint32_t _circlesCount = 0;
	float _maxValue = 0.0f;
};

class ShadowTrianglesAttachmentHandle : public BufferAttachmentHandle {
public:
	virtual ~ShadowTrianglesAttachmentHandle();

	void allocateBuffer(DeviceFrameHandle *, const gl::glsl::ShadowData &);

	virtual bool isDescriptorDirty(const PassHandle &, const PipelineDescriptor &,
			uint32_t, bool isExternal) const override;

	virtual bool writeDescriptor(const QueuePassHandle &, DescriptorBufferInfo &) override;

	const Rc<DeviceBuffer> &getTriangles() const { return _triangles; }
	const Rc<DeviceBuffer> &getCircles() const { return _circles; }
	const Rc<DeviceBuffer> &getGridSize() const { return _gridSize; }
	const Rc<DeviceBuffer> &getGridIndex() const { return _gridIndex; }

protected:
	Rc<DeviceBuffer> _triangles;
	Rc<DeviceBuffer> _circles;
	Rc<DeviceBuffer> _gridSize;
	Rc<DeviceBuffer> _gridIndex;
};

class ShadowImageArrayAttachmentHandle : public ImageAttachmentHandle {
public:
	virtual ~ShadowImageArrayAttachmentHandle() { }

	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	gl::ImageInfo getImageInfo() const { return _currentImageInfo; }
	float getShadowDensity() const { return _shadowDensity; }

	virtual bool isAvailable(const FrameQueue &) const override;

protected:
	float _shadowDensity = 1.0f;
	gl::ImageInfo _currentImageInfo;
};

class ShadowSdfImageAttachmentHandle : public ImageAttachmentHandle {
public:
	virtual ~ShadowSdfImageAttachmentHandle() { }

	virtual void submitInput(FrameQueue &, Rc<gl::AttachmentInputData> &&, Function<void(bool)> &&) override;

	gl::ImageInfo getImageInfo() const { return _currentImageInfo; }
	float getShadowDensity() const { return _shadowDensity; }
	float getSceneDensity() const { return _sceneDensity; }

protected:
	float _sceneDensity = 1.0f;
	float _shadowDensity = 1.0f;
	gl::ImageInfo _currentImageInfo;
};

class ShadowPassHandle : public QueuePassHandle {
public:
	virtual ~ShadowPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	const ShadowLightDataAttachmentHandle *_lightsBuffer = nullptr;
	const ShadowVertexAttachmentHandle *_vertexBuffer = nullptr;
	const ShadowTrianglesAttachmentHandle *_trianglesBuffer = nullptr;
	const ShadowImageArrayAttachmentHandle *_arrayAttachment = nullptr;

	uint32_t _gridCellSize = 32;
};

}

#endif /* XENOLITH_GL_VK_RENDERER_XLVKSHADOWRENDERPASS_H_ */
