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

#ifndef XENOLITH_GL_COMMON_XLGLDEVICE_H_
#define XENOLITH_GL_COMMON_XLGLDEVICE_H_

#include "XLGlRenderQueue.h"
#include "XLGlMaterial.h"
#include "XLGlFrameEmitter.h"
#include "XLEventHeader.h"
#include <deque>

namespace stappler::xenolith::gl {

class Resource;
class Instance;
class View;
class RenderQueue;
class ObjectInterface;
class Loop;
class FrameHandle;

class Device : public Ref {
public:
	Device();
	virtual ~Device();

	virtual bool init(const Instance *instance);

	virtual void begin(const Application *, thread::TaskQueue &);
	virtual void end(thread::TaskQueue &);
	virtual void waitIdle();

	virtual void defineSamplers(Vector<SamplerInfo> &&);

	// release any external resources
	virtual void invalidateFrame(FrameHandle &);

	Rc<Shader> getProgram(StringView);
	Rc<Shader> addProgram(Rc<Shader>);

	void addObject(ObjectInterface *);
	void removeObject(ObjectInterface *);

	uint32_t getSamplersCount() const { return _samplersCount; }
	bool isSamplersCompiled() const { return _samplersCompiled; }

	uint32_t getTextureLayoutImagesCount() const { return _textureLayoutImagesCount; }

	virtual void onLoopStarted(gl::Loop &);
	virtual void onLoopEnded(gl::Loop &);

	virtual bool supportsUpdateAfterBind(gl::DescriptorType) const;

	virtual Rc<gl::ImageObject> getEmptyImageObject() const = 0;
	virtual Rc<gl::ImageObject> getSolidImageObject() const = 0;

	virtual const Vector<ImageFormat> &getSupportedDepthStencilFormat() const = 0;

	virtual Rc<gl::FrameHandle> makeFrame(gl::Loop &, Rc<gl::FrameRequest> &&, uint64_t gen);

	virtual Rc<Framebuffer> makeFramebuffer(const gl::RenderPassData *, SpanView<Rc<gl::ImageView>>, Extent2);
	virtual Rc<ImageAttachmentObject> makeImage(const gl::ImageAttachment *, Extent3);

protected:
	friend class Loop;

	virtual void compileResource(gl::Loop &loop, const Rc<Resource> &req, Function<void(bool)> && = nullptr);
	virtual void compileRenderQueue(gl::Loop &loop, const Rc<RenderQueue> &req, Function<void(bool)> &&);

	virtual void compileMaterials(gl::Loop &loop, Rc<MaterialInputData> &&);

	virtual void compileImage(gl::Loop &loop, const Rc<DynamicImage> &, Function<void(bool)> &&) = 0;

	virtual void compileSamplers(thread::TaskQueue &q, bool force = true) = 0;

	void clearShaders();
	void invalidateObjects();

	bool _started = false;
	const Instance *_glInstance = nullptr;
	Mutex _mutex;

	Map<String, Rc<Shader>> _shaders;

	std::unordered_set<ObjectInterface *> _objects;

	Vector<SamplerInfo> _samplersInfo;

	uint32_t _samplersCount = 0;
	bool _samplersCompiled = false;
	uint32_t _textureLayoutImagesCount = 0;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLDEVICE_H_ */
