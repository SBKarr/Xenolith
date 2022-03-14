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

#include "XLResourceCache.h"
#include "XLFontFace.h"

namespace stappler::xenolith::font {

class FontFaceObject;
class FontFaceData;
class FontLibrary;

using FontLayoutId = layout::FontLayoutId;
using FontParameters = layout::style::FontStyleParameters;
using FontCharString = layout::FontCharString;

class FontController : public layout::FormatterSourceInterface {
public:
	static EventHeader onLoaded;
	static EventHeader onFontSourceUpdated;

	struct FontQuery {
		String name;
		String fontFilePath;
		Bytes fontMemoryData;
		BytesView fontExternalData;
		font::SystemFontName systemFont = font::SystemFontName::None;

		FontQuery(StringView name, BytesView data)
		: name(name.str()), fontExternalData(data) { }

		FontQuery(StringView name, Bytes && data)
		: name(name.str()), fontMemoryData(move(data)) { }

		FontQuery(StringView name, FilePath data)
		: name(name.str()), fontFilePath(data.get().str()) { }

		FontQuery(font::SystemFontName font)
		: name(getSystemFontName(font).str()), systemFont(font) { }
	};

	struct FamilyQuery {
		String family;
		FontStyle style;
		FontWeight weight;
		FontStretch stretch;
		Vector<String> sources;
		Vector<Pair<FontSize, FontCharString>> chars;
	};

	struct Query {
		Vector<FontQuery> dataQueries;
		Vector<FamilyQuery> familyQueries;
	};

	virtual ~FontController();

	bool init(const Rc<FontLibrary> &);

	void addFont(StringView family, FontStyle, FontWeight, FontStretch, Rc<FontFaceData> &&, bool front = false);
	void addFont(StringView family, FontStyle, FontWeight, FontStretch, Vector<Rc<FontFaceData>> &&, bool front = false);

	bool isLoaded() const { return _loaded; }
	const Rc<gl::DynamicImage> &getImage() const { return _image; }
	const Rc<Texture> &getTexture() const { return _texture; }

	virtual FontLayoutId getLayout(const FontParameters &f, float scale) override;
	virtual void addString(FontLayoutId, const FontCharString &) override;
	virtual uint16_t getFontHeight(FontLayoutId) override;
	virtual int16_t getKerningAmount(FontLayoutId, char16_t first, char16_t second, uint16_t face) const override;
	virtual Metrics getMetrics(FontLayoutId) override;
	virtual CharLayout getChar(FontLayoutId, char16_t, uint16_t &face) override;
	virtual StringView getFontName(FontLayoutId) override;

	FontCharLayout getFullChar(FontLayoutId, char16_t, uint16_t &face);
	void addTextureChars(FontLayoutId, SpanView<layout::CharSpec>);

	uint32_t getFamilyIndex(StringView) const;
	StringView getFamilyName(uint32_t idx) const;

	void update();

protected:
	friend class FontLibrary;

	void setImage(Rc<gl::DynamicImage> &&);
	void setLoaded(bool);

	class FontSizedLayout;
	class FontLayout;

	void updateTexture(Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&);

	FontLayout * getFontLayout(const FontParameters &style);

	bool _loaded = false;
	String _defaultFontFamily = "default";
	Rc<Texture> _texture;
	Rc<gl::DynamicImage> _image;
	Rc<FontLibrary> _library;

	Vector<StringView> _familiesNames;
	Map<StringView, Vector<FontLayout *>> _families;
	HashMap<StringView, Rc<FontLayout>> _layouts;
	Vector<FontSizedLayout *> _sizes;

	std::atomic<uint16_t> _nextId = 1;
	bool _dirty = false;
	mutable Mutex _mutex;
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

	Rc<FontFaceData> openFontData(StringView, const Callback<FontData()> & = nullptr);
	Rc<FontFaceData> openFontData(SystemFontName);

	Rc<FontFaceObject> openFontFace(SystemFontName, FontSize);
	Rc<FontFaceObject> openFontFace(StringView, FontSize, const Callback<FontData()> &);
	Rc<FontFaceObject> openFontFace(const Rc<FontFaceData> &, FontSize);

	void update();

	Rc<FontController> acquireController(StringView);
	Rc<FontController> acquireController(StringView, FontController::Query &&query);

	void updateImage(const Rc<gl::DynamicImage> &, Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&);

protected:
	FT_Face newFontFace(BytesView);
	void doneFontFace(FT_Face);

	void onActivated();

	struct ImageQuery {
		Rc<gl::DynamicImage> image;
		Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> chars;
	};

	bool _active = false;

	Mutex _mutex;
	uint16_t _nextId = 0;
	Map<StringView, Rc<FontFaceObject>> _faces;
	Map<StringView, Rc<FontFaceData>> _data;
	FT_Library _library = nullptr;

	const Application *_application = nullptr;
	Rc<gl::Loop> _loop; // for texture streaming
	Rc<gl::RenderQueue> _queue;
	Vector<ImageQuery> _pendingImageQueries;
};

}

#endif /* XENOLITH_FEATURES_FONT_XLFONTLIBRARY_H_ */
