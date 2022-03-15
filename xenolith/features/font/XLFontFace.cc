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

static CharGroupId getCharGroupForChar(char16_t c) {
	using namespace chars;
	if (CharGroup<char16_t, CharGroupId::Numbers>::match(c)) {
		return CharGroupId::Numbers;
	} else if (CharGroup<char16_t, CharGroupId::Latin>::match(c)) {
		return CharGroupId::Latin;
	} else if (CharGroup<char16_t, CharGroupId::Cyrillic>::match(c)) {
		return CharGroupId::Cyrillic;
	} else if (CharGroup<char16_t, CharGroupId::Currency>::match(c)) {
		return CharGroupId::Currency;
	} else if (CharGroup<char16_t, CharGroupId::GreekBasic>::match(c)) {
		return CharGroupId::GreekBasic;
	} else if (CharGroup<char16_t, CharGroupId::Math>::match(c)) {
		return CharGroupId::Math;
	} else if (CharGroup<char16_t, CharGroupId::TextPunctuation>::match(c)) {
		return CharGroupId::TextPunctuation;
	}
	return CharGroupId::None;
}

bool FontFaceData::init(StringView name, BytesView data, bool persistent) {
	if (persistent) {
		_view = data;
		_persistent = true;
		_name = name.str();
		return true;
	} else {
		return init(name, data.bytes());
	}
}

bool FontFaceData::init(StringView name, Bytes &&data) {
	_persistent = false;
	_data = move(data);
	_view = _data;
	_name = name.str();
	return true;
}

bool FontFaceData::init(StringView name, Function<Bytes()> &&cb) {
	_persistent = true;
	_data = cb();
	_view = _data;
	_name = name.str();
	return true;
}

BytesView FontFaceData::getView() const {
	return _view;
}

FontFaceObject::~FontFaceObject() { }

bool FontFaceObject::init(StringView name, const Rc<FontFaceData> &data, FT_Face face, FontSize fontSize, uint16_t id) {
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

	_metrics.size = fontSize.get();
	_metrics.height = face->size->metrics.height >> 6;
	_metrics.ascender = face->size->metrics.ascender >> 6;
	_metrics.descender = face->size->metrics.descender >> 6;
	_metrics.underlinePosition = face->underline_position >> 6;
	_metrics.underlineThickness = face->underline_thickness >> 6;

	_name = name.str();
	_id = id;
	_data = data;
	_size = fontSize;
	_face = face;

	return true;
}

bool FontFaceObject::acquireTexture(char16_t theChar, const Callback<void(uint8_t *, uint32_t width, uint32_t rows, int pitch)> &cb) {
	std::unique_lock<Mutex> lock(_faceMutex);

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

bool FontFaceObject::addChars(const Vector<char16_t> &chars, bool expand, Vector<char16_t> *failed) {
	bool updated = false;
	uint32_t mask = 0;

	// for some chars, we add full group, not only requested char
	for (auto &c : chars) {
		if (expand) {
			auto g = getCharGroupForChar(c);
			if (g != CharGroupId::None) {
				if ((mask & toInt(g)) == 0) {
					mask |= toInt(g);
					if (addCharGroup(g, failed)) {
						updated = true;
					}
					continue;
				}
			}
		}

		if (!addChar(c, updated) && failed) {
			mem_std::emplace_ordered(*failed, c);
		}
	}
	return updated;
}

bool FontFaceObject::addCharGroup(CharGroupId g, Vector<char16_t> *failed) {
	bool updated = false;
	using namespace chars;
	auto f = [&] (char16_t c) {
		if (!addChar(c, updated) && failed) {
			mem_std::emplace_ordered(*failed, c);
		}
	};

	switch (g) {
	case CharGroupId::Numbers: CharGroup<char16_t, CharGroupId::Numbers>::foreach(f); break;
	case CharGroupId::Latin: CharGroup<char16_t, CharGroupId::Latin>::foreach(f); break;
	case CharGroupId::Cyrillic: CharGroup<char16_t, CharGroupId::Cyrillic>::foreach(f); break;
	case CharGroupId::Currency: CharGroup<char16_t, CharGroupId::Currency>::foreach(f); break;
	case CharGroupId::GreekBasic: CharGroup<char16_t, CharGroupId::GreekBasic>::foreach(f); break;
	case CharGroupId::Math: CharGroup<char16_t, CharGroupId::Math>::foreach(f); break;
	case CharGroupId::TextPunctuation: CharGroup<char16_t, CharGroupId::TextPunctuation>::foreach(f); break;
	default: break;
	}
	return updated;
}

bool FontFaceObject::addRequiredChar(char16_t ch) {
	std::unique_lock<Mutex> lock(_requiredMutex);
	return mem_std::emplace_ordered(_required, ch);
}

Vector<char16_t> FontFaceObject::getRequiredChars() const {
	std::unique_lock<Mutex> lock(_requiredMutex);
	return _required;
}

CharLayout FontFaceObject::getChar(char16_t c) const {
	std::unique_lock<Mutex> lock(_charsMutex);
	auto l = _chars.get(c);
	if (l && l->layout.charID == c) {
		return l->layout;
	}
	return CharLayout{0};
}

FontCharLayout FontFaceObject::getFullChar(char16_t c) const {
	std::unique_lock<Mutex> lock(_charsMutex);
	auto l = _chars.get(c);
	if (l && l->layout.charID == c) {
		return *l;
	}
	return FontCharLayout();
}

int16_t FontFaceObject::getKerningAmount(char16_t first, char16_t second) const {
	std::unique_lock<Mutex> lock(_charsMutex);
	uint32_t key = (first << 16) | (second & 0xffff);
	auto it = _kerning.find(key);
	if (it != _kerning.end()) {
		return it->second;
	}
	return 0;
}

bool FontFaceObject::addChar(char16_t theChar, bool &updated) {
	std::unique_lock<Mutex> charsLock(_charsMutex);
	auto value = _chars.get(theChar);
	if (value) {
		if (value->layout.charID == theChar) {
			return true;
		} else if (value->layout.charID == char16_t(0xFFFF)) {
			return false;
		}
	}

	std::unique_lock<Mutex> faceLock(_faceMutex);
	int cIdx = FT_Get_Char_Index(_face, theChar);
	if (!cIdx) {
		_chars.emplace(theChar, FontCharLayout{ CharLayout{char16_t(0xFFFF), 0, 0, 0}, 0, 0 });
		return false;
	}

	auto err = FT_Load_Glyph(_face, cIdx, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
	if (err != FT_Err_Ok) {
		_chars.emplace(theChar, FontCharLayout{ CharLayout{char16_t(0xFFFF), 0, 0, 0}, 0, 0 });
		return false;
	}

	// store result in the passed rectangle
	_chars.emplace(theChar, FontCharLayout{ CharLayout{theChar,
		static_cast<int16_t>(_face->glyph->metrics.horiBearingX >> 6),
		static_cast<int16_t>(- (_face->glyph->metrics.horiBearingY >> 6)),
		static_cast<uint16_t>(_face->glyph->metrics.horiAdvance >> 6)},
		static_cast<uint16_t>(_face->glyph->metrics.width >> 6),
		static_cast<uint16_t>(_face->glyph->metrics.height >> 6)});

	if (!chars::isspace(theChar)) {
		updated = true;
	}

	if (FT_HAS_KERNING(_face)) {
		_chars.foreach([&] (const FontCharLayout & it) {
			if (it.layout.charID == 0 || it.layout.charID == char16_t(0xFFFF)) {
				return;
			}

			if (it.layout.charID != theChar) {
				FT_Vector kerning;
				auto err = FT_Get_Kerning(_face, cIdx, cIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(theChar << 16 | (it.layout.charID & 0xffff), value);
					}
				}
			} else {
				auto kIdx = FT_Get_Char_Index(_face, it.layout.charID);

				FT_Vector kerning;
				auto err = FT_Get_Kerning(_face, cIdx, kIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(theChar << 16 | (it.layout.charID & 0xffff), value);
					}
				}

				err = FT_Get_Kerning(_face, kIdx, cIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(it.layout.charID << 16 | (theChar & 0xffff), value);
					}
				}
			}
		});
	}
	return true;
}

}
