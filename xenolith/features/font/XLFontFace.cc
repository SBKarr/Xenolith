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

#include "XLFontFace.h"

namespace stappler::xenolith::font {

bool FontFaceData::init(BytesView data, bool persistent) {
	if (persistent) {
		_view = data;
		_persisent = true;
		return true;
	} else {
		return init(data.bytes());
	}
}

bool FontFaceData::init(Bytes &&data) {
	_persisent = false;
	_data = move(data);
	_view = _data;
	return true;
}

FontFaceObject::~FontFaceObject() { }

bool FontFaceObject::init(const Rc<FontFaceData> &data, FT_Face face, FontSize fontSize, uint16_t id) {
		//we want to use unicode
	auto err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
	if (err != FT_Err_Ok) {
		return false;
	}

	// set the requested font size
	err = FT_Set_Pixel_Sizes(face, fontSize.get(), fontSize.get());
	if (err != FT_Err_Ok) {
		return false;
	}

	_id = id;
	_data = data;
	_size = fontSize;
	_face = face;

	return true;
}

bool FontFaceObject::acquireTexture(char16_t theChar, const Callback<void(uint8_t *, uint32_t width, uint32_t rows, int pitch)> &cb) {
	std::unique_lock<Mutex> lock(_mutex);

	int glyph_index = FT_Get_Char_Index(_face, theChar);
	if (!glyph_index) {
		return false;
	}

	auto err = FT_Load_Glyph(_face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_RENDER);
	if (err != FT_Err_Ok) {
		return false;
	}

	//log::format("Texture", "%s: %d '%s'", data.layout->getName().c_str(), theChar.charID, string::toUtf8(theChar.charID).c_str());

	if (_face->glyph->bitmap.buffer != nullptr) {
		if (_face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
			cb(_face->glyph->bitmap.buffer, _face->glyph->bitmap.width, _face->glyph->bitmap.rows, _face->glyph->bitmap.pitch);
			return true;
		}
	} else {
		if (!string::isspace(theChar) && theChar != char16_t(0x0A)) {
			log::format("Font", "error: no bitmap for (%d) '%s'", theChar, string::toUtf8(theChar).data());
		}
	}
	return false;
}

}
