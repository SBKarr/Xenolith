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

#ifndef XENOLITH_FEATURES_FONT_XLFONTLIBRARY_H_
#define XENOLITH_FEATURES_FONT_XLFONTLIBRARY_H_

#include "XLDefine.h"
#include "XLFontSourceSystem.h"
#include "XLResourceCache.h"

typedef struct FT_LibraryRec_ * FT_Library;
typedef struct FT_FaceRec_ * FT_Face;

namespace stappler::xenolith::font {

using FontSize = layout::style::FontSize;

class FontFaceObject;
class FontFaceData;
class FontLibrary;

class FontController : public Ref {
public:
	virtual ~FontController();

	bool init(const Rc<FontLibrary> &, Rc<gl::DynamicImage> &&);

	void updateTexture(Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&);

	const Rc<gl::DynamicImage> &getImage() const { return _image; }
	const Rc<Texture> &getTexture() const { return _texture; }

protected:
	Rc<Texture> _texture;
	Rc<gl::DynamicImage> _image;
	Rc<FontLibrary> _library;
};

class FontLibrary : public Ref {
public:
	struct FontData {
		bool persistent;
		BytesView view;
		Bytes bytes;

		FontData(BytesView v, bool p) : persistent(p) {
			if (persistent) {
				view = v;
			} else {
				bytes = v.bytes();
				view = bytes;
			}
		}
		FontData(Bytes &&b) : persistent(false), bytes(move(b)) {
			view = bytes;
		}
	};

	FontLibrary();
	virtual ~FontLibrary();

	bool init(const Rc<gl::Loop> &, Rc<gl::RenderQueue> &&);

	// callback returns <view, isPersistent>
	Rc<FontFaceObject> openFontFace(SystemFontName, FontSize);
	Rc<FontFaceObject> openFontFace(StringView, FontSize, const Callback<FontData()> &);

	void update();

	void acquireController(StringView, Function<void(Rc<FontController> &&)> &&);

	void updateImage(const Rc<gl::DynamicImage> &, Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&);

protected:
	FT_Face newFontFace(BytesView);
	void doneFontFace(FT_Face);

	void onActivated();
	void runAcquire(StringView, Function<void(Rc<FontController> &&)> &&);

	bool _active = false;

	Mutex _mutex;
	uint16_t _nextId = 0;
	Map<String, Rc<FontFaceObject>> _faces;
	Map<String, Rc<FontFaceData>> _data;
	FT_Library _library = nullptr;

	Rc<gl::Loop> _loop; // for texture streaming
	Rc<gl::RenderQueue> _queue;

	Vector<Pair<String, Function<void(Rc<FontController> &&)>>> _pending;
};

}

#endif /* XENOLITH_FEATURES_FONT_XLFONTLIBRARY_H_ */
