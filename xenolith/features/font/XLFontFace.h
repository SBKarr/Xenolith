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

#ifndef XENOLITH_FEATURES_FONT_XLFONTFACE_H_
#define XENOLITH_FEATURES_FONT_XLFONTFACE_H_

#include "XLFontLibrary.h"

namespace stappler::xenolith::font {

class FontFaceData : public Ref {
public:
	virtual ~FontFaceData() { }

	bool init(BytesView, bool);
	bool init(Bytes &&);

	BytesView getView() const { return _view; }

protected:
	bool _persisent = false;
	BytesView _view;
	Bytes _data;
};

class FontFaceObject : public Ref {
public:
	virtual ~FontFaceObject();

	bool init(const Rc<FontFaceData> &, FT_Face, FontSize, uint16_t);

	uint16_t getId() const { return _id; }
	FT_Face getFace() const { return _face; }

	bool acquireTexture(char16_t, const Callback<void(uint8_t *, uint32_t width, uint32_t rows, int pitch)> &);

protected:
	Rc<FontFaceData> _data;
	uint16_t _id = 0;
	FontSize _size;
	FT_Face _face = nullptr;
	Mutex _mutex;
};

}

#endif /* XENOLITH_FEATURES_FONT_XLFONTFACE_H_ */
