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

#ifndef XENOLITH_FEATURES_FONT_XLFONTLAYOUT_H_
#define XENOLITH_FEATURES_FONT_XLFONTLAYOUT_H_

#include "XLDefine.h"
#include "XLFontFace.h"

namespace stappler::xenolith::font {

class FontLibrary;

class FontLayout : public Ref {
public:
	static String constructName(StringView, const FontSpecializationVector &);

	virtual ~FontLayout() { }
	FontLayout() { }

	bool init(String &&, StringView family, FontSpecializationVector &&, Rc<FontFaceData> &&data, FontLibrary *);
	bool init(String &&, StringView family, FontSpecializationVector &&, Vector<Rc<FontFaceData>> &&data, FontLibrary *);

	void touch(uint64_t clock, bool persistent);

	uint64_t getAccessTime() const { return _accessTime; }
	bool isPersistent() const { return _persistent; }

	StringView getName() const { return _name; }
	StringView getFamily() const { return _family; }

	const FontSpecializationVector &getSpec() const { return _spec; }

	size_t getFaceCount() const;

	Rc<FontFaceData> getSource(size_t) const;
	FontLibrary *getLibrary() const { return _library; }

	bool addString(const FontCharString &, Vector<char16_t> &failed);
	uint16_t getFontHeight() const;
	int16_t getKerningAmount(char16_t first, char16_t second, uint16_t face) const;
	Metrics getMetrics() const;
	CharLayout getChar(char16_t, uint16_t &face) const;

	bool addTextureChars(SpanView<CharSpec>) const;

	const Vector<Rc<FontFaceObject>> &getFaces() const;

protected:
	// void updateSizedFaces(size_t prefix, Vector<FontSizedLayout *> &sizes);

	std::atomic<uint64_t> _accessTime;
	std::atomic<bool> _persistent = false;

	String _name;
	String _family;
	Metrics _metrics;
	FontSpecializationVector _spec;
	Vector<Rc<FontFaceData>> _sources;
	Vector<Rc<FontFaceObject>> _faces;
	FontLibrary *_library = nullptr;
	mutable std::shared_mutex _mutex;
};

// TODO: should be immutable object
/*class FontSizedLayout : public Ref {
public:
	virtual ~FontSizedLayout() { }
	FontSizedLayout() { }

	bool init(FontSize, String &&, FontLayoutId, FontController::FontLayout *, Rc<FontFaceObject> &&);
	bool init(FontSize, String &&, FontLayoutId, FontController::FontLayout *, Vector<Rc<FontFaceObject>> &&);

	FontSize getSize() const { return _size; }
	StringView getName() const { return _name; }
	FontLayoutId getId() const { return _id; }
	FontController::FontLayout *getLayout() const { return _layout; }
	const Vector<Rc<FontFaceObject>> &getFaces() const { return _faces; }

	bool isComplete() const;


	// void prefixFonts(size_t count);

protected:
	FontSize _size;
	String _name;
	FontController::FontLayout *_layout = nullptr;
};*/

}

#endif /* XENOLITH_FEATURES_FONT_XLFONTLAYOUT_H_ */
