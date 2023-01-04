/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_FEATURES_FONT_XLFONTFACE_H_
#define XENOLITH_FEATURES_FONT_XLFONTFACE_H_

#include "XLFontStyle.h"
#include <shared_mutex>

typedef struct FT_LibraryRec_ * FT_Library;
typedef struct FT_FaceRec_ * FT_Face;

namespace stappler::xenolith::font {

template <typename Value>
struct FontCharStorage {
	using CellType = std::array<Value, 256>;

	FontCharStorage() {
		cells.fill(nullptr);
	}

	~FontCharStorage() {
		for (auto &it : cells) {
			if (it) {
				delete it;
				it = nullptr;
			}
		}
	}

	void emplace(char16_t ch, Value &&value) {
		auto cellId = ch / 256;
		if (!cells[cellId]) {
			cells[cellId] = new CellType;
			memset(cells[cellId]->data(), 0, cells[cellId]->size() * sizeof(Value));
		}

		(*cells[cellId])[ch % 256] = move(value);
	}

	const Value *get(char16_t ch) const {
		auto cellId = ch / 256;
		if (!cells[cellId]) {
			return nullptr;
		}
		return &(cells[cellId]->at(ch % 256));
	}

	template <typename Callback>
	void foreach(const Callback &cb) {
		for (auto &it : cells) {
			if (it) {
				for (auto &iit : *it) {
					cb(iit);
				}
			}
		}
	}

	std::array<CellType *, 256> cells;
};

class FontFaceData : public Ref {
public:
	virtual ~FontFaceData() { }

	bool init(StringView, BytesView, bool);
	bool init(StringView, Bytes &&);
	bool init(StringView, Function<Bytes()> &&);

	void inspectVariableFont(FontLayoutParameters params, FT_Face);

	StringView getName() const { return _name; }
	BytesView getView() const;

	FontVariableAxis getVariableAxis() const { return _variableAxis; }

	FontWeight getWeightMin() const { return _weightMin; }
	FontWeight getWeightMax() const { return _weightMax; }
	FontStretch getStretchMin() const { return _stretchMin; }
	FontStretch getStretchMax() const { return _stretchMax; }
	FontStyle getSlantMin() const { return _slantMin; }
	FontStyle getSlantMax() const { return _slantMax; }
	FontGrade getGradeMin() const { return _gradeMin; }
	FontGrade getGradeMax() const { return _gradeMax; }
	uint32_t getOpticalSizeMin() const { return _opticalSizeMin; }
	uint32_t getOpticalSizeMax() const { return _opticalSizeMax; }
	uint32_t getItalicMin() const { return _italicMin; }
	uint32_t getItalicMax() const { return _italicMax; }

	FontSpecializationVector getSpecialization(const FontSpecializationVector &) const;

protected:
	bool _persistent = false;
	String _name;
	BytesView _view;
	Bytes _data;
	FontVariableAxis _variableAxis = FontVariableAxis::None;
	FontWeight _weightMin;
	FontWeight _weightMax;
	FontStretch _stretchMin;
	FontStretch _stretchMax;
	FontStyle _slantMin;
	FontStyle _slantMax;
	uint32_t _opticalSizeMin = 0;
	uint32_t _opticalSizeMax = 0;
	uint32_t _italicMin = 0;
	uint32_t _italicMax = 0;
	FontGrade _gradeMin;
	FontGrade _gradeMax;
	FontLayoutParameters _params;
};

class FontFaceObject : public Ref {
public:
	virtual ~FontFaceObject();

	bool init(StringView, const Rc<FontFaceData> &, FT_Face, const FontSpecializationVector &, uint16_t);

	StringView getName() const { return _name; }
	uint16_t getId() const { return _id; }
	FT_Face getFace() const { return _face; }
	const Rc<FontFaceData> &getData() const { return _data; }
	const FontSpecializationVector &getSpec() const { return _spec; }

	bool acquireTexture(char16_t, const Callback<void(const CharTexture &)> &);
	bool acquireTextureUnsafe(char16_t, const Callback<void(const CharTexture &)> &);

	// returns true if updated
	bool addChars(const Vector<char16_t> &chars, bool expand, Vector<char16_t> *failed);
	bool addCharGroup(CharGroupId, Vector<char16_t> *failed);

	bool addRequiredChar(char16_t);

	Vector<char16_t> getRequiredChars() const;
	CharLayout getChar(char16_t c) const;
	int16_t getKerningAmount(char16_t first, char16_t second) const;

	Metrics getMetrics() const { return _metrics; }

protected:
	bool addChar(char16_t, bool &updated);

	String _name;
	Rc<FontFaceData> _data;
	uint16_t _id = 0;
	FT_Face _face = nullptr;
	FontSpecializationVector _spec;
	Metrics _metrics;
	Vector<char16_t> _required;
	FontCharStorage<CharLayout> _chars;
	HashMap<uint32_t, int16_t> _kerning;
	Mutex _faceMutex;
	mutable std::shared_mutex _charsMutex;
	mutable Mutex _requiredMutex;
};

}

#endif /* XENOLITH_FEATURES_FONT_XLFONTFACE_H_ */
