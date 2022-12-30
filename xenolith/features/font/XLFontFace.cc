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

static constexpr uint32_t getAxisTag(char c1, char c2, char c3, char c4) {
	return uint32_t(c1 & 0xFF) << 24 | uint32_t(c2 & 0xFF) << 16 | uint32_t(c3 & 0xFF) << 8 | uint32_t(c4 & 0xFF);
}

static constexpr uint32_t getAxisTag(const char c[4]) {
	return getAxisTag(c[0], c[1], c[2], c[3]);
}

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
		_name = name.str<Interface>();
		return true;
	} else {
		return init(name, data.bytes<Interface>());
	}
}

bool FontFaceData::init(StringView name, Bytes &&data) {
	_persistent = false;
	_data = move(data);
	_view = _data;
	_name = name.str<Interface>();
	return true;
}

bool FontFaceData::init(StringView name, Function<Bytes()> &&cb) {
	_persistent = true;
	_data = cb();
	_view = _data;
	_name = name.str<Interface>();
	return true;
}

void FontFaceData::inspectVariableFont(FontLayoutParameters params, FT_Face face) {
	FT_MM_Var *masters = nullptr;
	FT_Get_MM_Var(face, &masters);

	if (masters) {
		for (FT_UInt i = 0; i < masters->num_axis; ++ i) {
			auto tag = masters->axis[i].tag;
			if (tag == getAxisTag("wght")) {
				_variableAxis |= FontVariableAxis::Weight;
				_weightMin = FontWeight(masters->axis[i].minimum >> 16);
				_weightMax = FontWeight(masters->axis[i].maximum >> 16);
			} else if (tag == getAxisTag("wdth")) {
				_variableAxis |= FontVariableAxis::Width;
				_stretchMin = FontStretch(masters->axis[i].minimum >> 15);
				_stretchMax = FontStretch(masters->axis[i].maximum >> 15);
			} else if (tag == getAxisTag("ital")) {
				_variableAxis |= FontVariableAxis::Italic;
				_italicMin = masters->axis[i].minimum;
				_italicMax = masters->axis[i].maximum;
			} else if (tag == getAxisTag("slnt")) {
				_variableAxis |= FontVariableAxis::Slant;
				_slantMin = FontStyle(masters->axis[i].minimum >> 10);
				_slantMax = FontStyle(masters->axis[i].maximum >> 10);
			} else if (tag == getAxisTag("opsz")) {
				_variableAxis |= FontVariableAxis::OpticalSize;
				_opticalSizeMin = masters->axis[i].minimum;
				_opticalSizeMax = masters->axis[i].maximum;
			}
			std::cout << "Variable axis: [" << masters->axis[i].tag << "] "
					<< (masters->axis[i].minimum >> 16) << " - " << (masters->axis[i].maximum >> 16)
					<< " def: "<< (masters->axis[i].def >> 16) << "\n";
		}
	}

	// apply static params
	if ((_variableAxis & FontVariableAxis::Weight) == FontVariableAxis::None) {
		_weightMin = _weightMax = params.fontWeight;
	}
	if ((_variableAxis & FontVariableAxis::Width) == FontVariableAxis::None) {
		_stretchMin = _stretchMax = params.fontStretch;
	}
	if ((_variableAxis & FontVariableAxis::Italic) == FontVariableAxis::None &&
			(_variableAxis & FontVariableAxis::Slant) == FontVariableAxis::None) {
		switch (params.fontStyle.get()) {
		case FontStyle::Normal.get(): break;
		case FontStyle::Italic.get(): _italicMin = _italicMax = 1; break;
		default: _slantMin = _slantMax = FontStyle::Oblique; break;
		}
	}

	_params = params;
}

BytesView FontFaceData::getView() const {
	return _view;
}

FontSpecializationVector FontFaceData::getSpecialization(const FontSpecializationVector &vec) const {
	FontSpecializationVector ret = vec;
	ret.fontStyle = _params.fontStyle;
	ret.fontStretch = _params.fontStretch;
	ret.fontWeight = _params.fontWeight;
	if ((_variableAxis & FontVariableAxis::Weight) != FontVariableAxis::None) {
		ret.fontWeight = math::clamp(vec.fontWeight, _weightMin, _weightMax);
	}
	if ((_variableAxis & FontVariableAxis::Stretch) != FontVariableAxis::None) {
		ret.fontStretch = math::clamp(vec.fontStretch, _stretchMin, _stretchMax);
	}

	if (ret.fontStyle != vec.fontStyle) {
		switch (vec.fontStyle.get()) {
		case FontStyle::Normal.get():
			if (_params.fontStyle == FontStyle::Italic
					&& (_variableAxis & FontVariableAxis::Italic) != FontVariableAxis::None
					&& _italicMin != _italicMax) {
				// we can disable italic
				ret.fontStyle = FontStyle::Normal;
			} else if (_params.fontStyle == FontStyle::Oblique
					&& (_variableAxis & FontVariableAxis::Slant) != FontVariableAxis::None
					&& _slantMin <= FontStyle::Normal && _slantMax >= FontStyle::Normal) {
				//  we can remove slant
				ret.fontStyle = math::clamp(FontStyle::Normal, _slantMin, _slantMax);
			}
			break;
		case FontStyle::Italic.get():
			// try true italic or slant emulation
			if ((_variableAxis & FontVariableAxis::Italic) != FontVariableAxis::None && _italicMin != _italicMax) {
				ret.fontStyle = FontStyle::Italic;
			} else if ((_variableAxis & FontVariableAxis::Slant) != FontVariableAxis::None) {
				ret.fontStyle = math::clamp(FontStyle::Oblique, _slantMin, _slantMax);
			}
			break;
		default:
			if ((_variableAxis & FontVariableAxis::Slant) != FontVariableAxis::None) {
				ret.fontStyle = math::clamp(vec.fontStyle, _slantMin, _slantMax);
			} else if ((_variableAxis & FontVariableAxis::Italic) != FontVariableAxis::None && _italicMin != _italicMax) {
				ret.fontStyle = FontStyle::Italic;
			}
			break;
		}
	}

	return ret;
}

FontFaceObject::~FontFaceObject() { }

bool FontFaceObject::init(StringView name, const Rc<FontFaceData> &data, FT_Face face, const FontSpecializationVector &spec, uint16_t id) {
	auto err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
	if (err != FT_Err_Ok) {
		return false;
	}

	if (data->getVariableAxis() != FontVariableAxis::None) {
		Vector<FT_Fixed> vector;

		FT_MM_Var *masters;
		FT_Get_MM_Var(face, &masters);

		if (masters) {
			for (FT_UInt i = 0; i < masters->num_axis; ++ i) {
				auto tag = masters->axis[i].tag;
				if (tag == getAxisTag("wght")) {
					vector.emplace_back(math::clamp(spec.fontWeight, data->getWeightMin(), data->getWeightMax()).get() << 16);
				} else if (tag == getAxisTag("wdth")) {
					vector.emplace_back(math::clamp(spec.fontStretch, data->getStretchMin(), data->getStretchMax()).get() << 15);
				} else if (tag == getAxisTag("ital")) {
					switch (spec.fontStyle.get()) {
					case FontStyle::Normal.get(): vector.emplace_back(data->getItalicMin()); break;
					case FontStyle::Italic.get(): vector.emplace_back(data->getItalicMax()); break;
					default:
						if ((data->getVariableAxis() & FontVariableAxis::Slant) != FontVariableAxis::None) {
							vector.emplace_back(data->getItalicMin()); // has true oblique
						} else {
							vector.emplace_back(data->getItalicMax());
						}
						break;
					}
				} else if (tag == getAxisTag("slnt")) {
					switch (spec.fontStyle.get()) {
					case FontStyle::Normal.get(): vector.emplace_back(0); break;
					case FontStyle::Italic.get():
						if ((data->getVariableAxis() & FontVariableAxis::Italic) != FontVariableAxis::None) {
							vector.emplace_back(masters->axis[i].def);
						} else {
							vector.emplace_back(math::clamp(FontStyle::Oblique, data->getSlantMin(), data->getSlantMax()).get() << 10);
						}
						break;
					default:
						vector.emplace_back(math::clamp(spec.fontStyle, data->getSlantMin(), data->getSlantMax()).get() << 10);
						break;
					}
				} else if (tag == getAxisTag("opsz")) {
					auto opticalSize = uint32_t(floorf(spec.fontSize.get() / spec.density)) << 16;
					vector.emplace_back(math::clamp(opticalSize, data->getOpticalSizeMin(), data->getOpticalSizeMax()));
				} else {
					vector.emplace_back(masters->axis[i].def);
				}
			}

			FT_Set_Var_Design_Coordinates(face, vector.size(), vector.data());
		}
	}

	// set the requested font size
	err = FT_Set_Pixel_Sizes(face, spec.fontSize.get(), spec.fontSize.get());
	if (err != FT_Err_Ok) {
		return false;
	}

	_spec = spec;
	_metrics.size = spec.fontSize.get();
	_metrics.height = face->size->metrics.height >> 6;
	_metrics.ascender = face->size->metrics.ascender >> 6;
	_metrics.descender = face->size->metrics.descender >> 6;
	_metrics.underlinePosition = face->underline_position >> 6;
	_metrics.underlineThickness = face->underline_thickness >> 6;

	_name = name.str<Interface>();
	_id = id;
	_data = data;
	_face = face;

	return true;
}

bool FontFaceObject::acquireTexture(char16_t theChar, const Callback<void(const CharTexture &)> &cb) {
	std::unique_lock<Mutex> lock(_faceMutex);

	return acquireTextureUnsafe(theChar, cb);
}

bool FontFaceObject::acquireTextureUnsafe(char16_t theChar, const Callback<void(const CharTexture &)> &cb) {
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
			cb(CharTexture{_id, theChar,
				static_cast<int16_t>(_face->glyph->metrics.horiBearingX >> 6),
				static_cast<int16_t>(- (_face->glyph->metrics.horiBearingY >> 6)),
				static_cast<uint16_t>(_face->glyph->metrics.width >> 6),
				static_cast<uint16_t>(_face->glyph->metrics.height >> 6),
				_face->glyph->bitmap.width,
				_face->glyph->bitmap.rows,
				_face->glyph->bitmap.pitch ? _face->glyph->bitmap.pitch : int(_face->glyph->bitmap.width),
				_face->glyph->bitmap.buffer
			});
			return true;
		}
	} else {
		if (!string::isspace(theChar) && theChar != char16_t(0x0A)) {
			log::format("Font", "error: no bitmap for (%d) '%s'", theChar, string::toUtf8<Interface>(theChar).data());
		}
	}
	return false;
}

bool FontFaceObject::addChars(const Vector<char16_t> &chars, bool expand, Vector<char16_t> *failed) {
	bool updated = false;
	uint32_t mask = 0;

	if constexpr (!config::FontPreloadGroups) {
		expand = false;
	}

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
	std::shared_lock lock(_charsMutex);
	auto l = _chars.get(c);
	if (l && l->charID == c) {
		return *l;
	}
	return CharLayout{0};
}

int16_t FontFaceObject::getKerningAmount(char16_t first, char16_t second) const {
	std::shared_lock lock(_charsMutex);
	uint32_t key = (first << 16) | (second & 0xffff);
	auto it = _kerning.find(key);
	if (it != _kerning.end()) {
		return it->second;
	}
	return 0;
}

bool FontFaceObject::addChar(char16_t theChar, bool &updated) {
	do {
		// try to get char with shared lock
		std::shared_lock charsLock(_charsMutex);
		auto value = _chars.get(theChar);
		if (value) {
			if (value->charID == theChar) {
				return true;
			} else if (value->charID == char16_t(0xFFFF)) {
				return false;
			}
		}
	} while (0);

	std::unique_lock charsUniqueLock(_charsMutex);
	auto value = _chars.get(theChar);
	if (value) {
		if (value->charID == theChar) {
			return true;
		} else if (value->charID == char16_t(0xFFFF)) {
			return false;
		}
	}

	std::unique_lock<Mutex> faceLock(_faceMutex);
	FT_UInt cIdx = FT_Get_Char_Index(_face, theChar);
	if (!cIdx) {
		_chars.emplace(theChar, CharLayout{char16_t(0xFFFF)});
		return false;
	}

	FT_Fixed advance;
	auto err = FT_Get_Advance(_face, cIdx, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP, &advance);
	if (err != FT_Err_Ok) {
		_chars.emplace(theChar, CharLayout{char16_t(0xFFFF)});
		return false;
	}

	/*auto err = FT_Load_Glyph(_face, cIdx, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
	if (err != FT_Err_Ok) {
		_chars.emplace(theChar, CharLayout{char16_t(0xFFFF)});
		return false;
	}*/

	// store result in the passed rectangle
	_chars.emplace(theChar, CharLayout{theChar,
		//static_cast<int16_t>(_face->glyph->metrics.horiBearingX >> 6),
		//static_cast<int16_t>(- (_face->glyph->metrics.horiBearingY >> 6)),
		static_cast<uint16_t>(advance >> 16),
		//static_cast<uint16_t>(_face->glyph->metrics.width >> 6),
		//static_cast<uint16_t>(_face->glyph->metrics.height >> 6)
		});

	if (!chars::isspace(theChar)) {
		updated = true;
	}

	if (FT_HAS_KERNING(_face)) {
		_chars.foreach([&] (const CharLayout & it) {
			if (it.charID == 0 || it.charID == char16_t(0xFFFF)) {
				return;
			}

			if (it.charID != theChar) {
				FT_Vector kerning;
				auto err = FT_Get_Kerning(_face, cIdx, cIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(theChar << 16 | (it.charID & 0xffff), value);
					}
				}
			} else {
				auto kIdx = FT_Get_Char_Index(_face, it.charID);

				FT_Vector kerning;
				auto err = FT_Get_Kerning(_face, cIdx, kIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(theChar << 16 | (it.charID & 0xffff), value);
					}
				}

				err = FT_Get_Kerning(_face, kIdx, cIdx, FT_KERNING_DEFAULT, &kerning);
				if (err == FT_Err_Ok) {
					auto value = (int16_t)(kerning.x >> 6);
					if (value != 0) {
						_kerning.emplace(it.charID << 16 | (theChar & 0xffff), value);
					}
				}
			}
		});
	}
	return true;
}

}
