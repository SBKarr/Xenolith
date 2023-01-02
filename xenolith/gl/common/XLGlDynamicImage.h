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

#ifndef XENOLITH_GL_COMMON_XLGLDYNAMICIMAGE_H_
#define XENOLITH_GL_COMMON_XLGLDYNAMICIMAGE_H_

#include "XLGlObject.h"

namespace stappler::xenolith::gl {

class Loop;
class DynamicImage;

struct DynamicImageInstance : public Ref {
	virtual ~DynamicImageInstance() { }

	ImageData data;
	Rc<Ref> userdata;
	Rc<DynamicImage> image = nullptr;
	uint32_t gen = 0;
};

class DynamicImage : public Ref {
public:
	class Builder;

	virtual ~DynamicImage();

	bool init(const Callback<bool(Builder &)> &);
	void finalize();

	Rc<DynamicImageInstance> getInstance();

	void updateInstance(Loop &, const Rc<ImageObject> &, Rc<ImageAtlas> && = Rc<ImageAtlas>(), Rc<Ref> &&userdata = Rc<Ref>(),
			const Vector<Rc<renderqueue::DependencyEvent>> & = Vector<Rc<renderqueue::DependencyEvent>>());

	void addTracker(const MaterialAttachment *);
	void removeTracker(const MaterialAttachment *);

	ImageInfo getInfo() const;
	Extent3 getExtent() const;

	// called when image compiled successfully
	void setImage(const Rc<ImageObject> &);
	void acquireData(const Callback<void(BytesView)> &);

protected:
	friend class Builder;

	mutable Mutex _mutex;
	String _keyData;
	Bytes _imageData;
	ImageData _data;
	Rc<DynamicImageInstance> _instance;
	Set<const MaterialAttachment *> _materialTrackers;
};

class DynamicImage::Builder {
public:
	const ImageData * setImageByRef(StringView key, ImageInfo &&, BytesView data, Rc<ImageAtlas> && = nullptr);
	const ImageData * setImage(StringView key, ImageInfo &&, FilePath data, Rc<ImageAtlas> && = nullptr);
	const ImageData * setImage(StringView key, ImageInfo &&, BytesView data, Rc<ImageAtlas> && = nullptr);
	const ImageData * setImage(StringView key, ImageInfo &&,
			Function<void(const ImageData::DataCallback &)> &&cb, Rc<ImageAtlas> && = nullptr);

protected:
	friend class DynamicImage;

	Builder(DynamicImage *);

	DynamicImage *_data = nullptr;
};

}

#endif /* XENOLITH_GL_COMMON_XLGLDYNAMICIMAGE_H_ */
