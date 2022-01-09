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

#include "XLFontFace.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace stappler::xenolith::font {

FontLibrary::FontLibrary() {
	FT_Init_FreeType( &_library );
}
FontLibrary::~FontLibrary() {
	if (_library) {
		FT_Done_FreeType(_library);
		_library = nullptr;
	}
}

Rc<FontFaceObject> FontLibrary::openFontFace(StringView dataName, FontSize size, const Callback<Bytes()> &dataCallback) {
	auto faceName = toString(dataName, "?size=", size.get());

	std::unique_lock<Mutex> lock(_mutex);
	do {
		auto it = _faces.find(faceName);
		if (it != _faces.end()) {
			return it->second;
		}
	} while (0);

	auto it = _data.find(dataName);
	if (it != _data.end()) {
		auto face = newFontFace(it->second->getView());
		auto ret = Rc<FontFaceObject>::create(it->second, face, size);
		if (ret) {
			_faces.emplace(faceName, ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	auto data = dataCallback();
	auto dataObject = Rc<FontFaceData>::create(move(data));
	if (dataObject) {
		_data.emplace(dataName.str(), dataObject);
		auto face = newFontFace(it->second->getView());
		auto ret = Rc<FontFaceObject>::create(it->second, face, size);
		if (ret) {
			_faces.emplace(faceName, ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	return nullptr;
}

void FontLibrary::update() {
	std::unique_lock<Mutex> lock(_mutex);

	do {
		auto it = _faces.begin();
		while (it != _faces.end()) {
			if (it->second->getReferenceCount() == 1) {
				doneFontFace(it->second->getFace());
				it = _faces.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);

	do {
		auto it = _data.begin();
		while (it != _data.end()) {
			if (it->second->getReferenceCount() == 1) {
				it = _data.erase(it);
			} else {
				++ it;
			}
		}
	} while (0);
}

FT_Face FontLibrary::newFontFace(BytesView data) {
	FT_Face ret = nullptr;
	FT_Error err = FT_Err_Ok;

	err = FT_New_Memory_Face(_library, data.data(), data.size(), 0, &ret );
	if (err != FT_Err_Ok) {
		return nullptr;
	}

	return ret;
}

void FontLibrary::doneFontFace(FT_Face face) {
	if (face) {
		FT_Done_Face(face);
	}
}

bool FontFaceData::init(Bytes &&data) {
	_data = move(data);
	return true;
}

FontFaceObject::~FontFaceObject() { }

bool FontFaceObject::init(const Rc<FontFaceData> &data, FT_Face face, FontSize fontSize) {
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
