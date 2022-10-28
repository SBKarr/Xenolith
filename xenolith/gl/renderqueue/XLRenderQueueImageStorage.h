/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEIMAGESTORAGE_H_
#define XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEIMAGESTORAGE_H_

#include "XLRenderQueue.h"

namespace stappler::xenolith::renderqueue {

class ImageStorage : public Ref {
public:
	virtual ~ImageStorage();

	virtual bool init(Rc<gl::ImageObject> &&);

	bool isCacheable() const;
	bool isSwapchainImage() const;

	virtual void cleanup();
	virtual void rearmSemaphores(gl::Loop &);
	virtual void releaseSemaphore(gl::Semaphore *);

	void invalidate();

	bool isReady() const { return _ready && !_invalid; }
	void setReady(bool);
	void waitReady(Function<void(bool)> &&);

	virtual bool isSemaphorePersistent() const { return true; }

	const Rc<gl::Semaphore> &getWaitSem() const;
	const Rc<gl::Semaphore> &getSignalSem() const;
	uint32_t getImageIndex() const;

	virtual gl::ImageInfoData getInfo() const;
	Rc<gl::ImageObject> getImage() const;

	void addView(const gl::ImageViewInfo &, Rc<gl::ImageView> &&);
	Rc<gl::ImageView> getView(const gl::ImageViewInfo &) const;
	virtual Rc<gl::ImageView> makeView(const gl::ImageViewInfo &);

	void setLayout(AttachmentLayout);
	AttachmentLayout getLayout() const;

	const Map<gl::ImageViewInfo, Rc<gl::ImageView>> &getViews() const { return _views; }

protected:
	void notifyReady();

	Rc<gl::ImageObject> _image;
	Rc<gl::Semaphore> _waitSem;
	Rc<gl::Semaphore> _signalSem;
	Map<gl::ImageViewInfo, Rc<gl::ImageView>> _views;

	bool _ready = true;
	bool _invalid = false;
	bool _isSwapchainImage = false;
	AttachmentLayout _layout = AttachmentLayout::Undefined;

	Vector<Function<void(bool)>> _waitReady;
};

}


#endif /* XENOLITH_GL_RENDERQUEUE_XLRENDERQUEUEIMAGESTORAGE_H_ */
