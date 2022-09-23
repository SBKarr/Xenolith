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

class FontController : public Ref {
public:
	static EventHeader onLoaded;
	static EventHeader onFontSourceUpdated;

	struct FontSource {
		String fontFilePath;
		Bytes fontMemoryData;
		BytesView fontExternalData;
		Function<Bytes()> fontCallback;
		Rc<FontFaceData> data;
	};

	struct FamilyQuery {
		String family;
		FontStyle style;
		FontWeight weight;
		FontStretch stretch;
		Vector<const FontSource *> sources;
		Vector<Pair<FontSize, FontCharString>> chars;
	};

	class Builder {
	public:
		struct Data;

		~Builder();

		Builder(StringView);

		Builder(Builder &&);
		Builder &operator=(Builder &&);

		Builder(const Builder &) = delete;
		Builder &operator=(const Builder &) = delete;

		StringView getName() const;

		const FontSource * addFontSource(StringView name, BytesView data);
		const FontSource * addFontSource(StringView name, Bytes && data);
		const FontSource * addFontSource(StringView name, FilePath data);
		const FontSource * addFontSource(StringView name, Function<Bytes()> &&cb);

		const FontSource *getFontSource(StringView) const;

		const FamilyQuery * addFontFaceQuery(StringView family, FontStyle, FontWeight, FontStretch, const FontSource *,
				Vector<Pair<FontSize,FontCharString>> &&chars = Vector<Pair<FontSize, FontCharString>>(), bool front = false);

		const FamilyQuery * addFontFaceQuery(StringView family, FontStyle, FontWeight, FontStretch, Vector<const FontSource *> &&,
				Vector<Pair<FontSize,FontCharString>> &&chars = Vector<Pair<FontSize, FontCharString>>(), bool front = false);

		bool addAlias(StringView newAlias, StringView familyName);

		Vector<const FamilyQuery *> getFontFamily(StringView family) const;
		Map<String, String> getAliases() const;

		Data *getData() const { return _data; }

	protected:
		void addSources(FamilyQuery *, Vector<const FontSource *> &&, bool front);
		void addChars(FamilyQuery *, Vector<Pair<FontSize, FontCharString>> &&);

		Data *_data;
	};

	virtual ~FontController();

	bool init(const Rc<FontLibrary> &);

	void addFont(StringView family, FontStyle, FontWeight, FontStretch, Rc<FontFaceData> &&, bool front = false);
	void addFont(StringView family, FontStyle, FontWeight, FontStretch, Vector<Rc<FontFaceData>> &&, bool front = false);

	// replaces previous alias
	bool addAlias(StringView newAlias, StringView familyName);

	bool isLoaded() const { return _loaded; }
	const Rc<gl::DynamicImage> &getImage() const { return _image; }
	const Rc<Texture> &getTexture() const { return _texture; }

	FontLayoutId getLayout(const FontParameters &f, float scale);
	void addString(FontLayoutId, const FontCharString &);
	uint16_t getFontHeight(FontLayoutId);
	int16_t getKerningAmount(FontLayoutId, char16_t first, char16_t second, uint16_t face) const;
	Metrics getMetrics(FontLayoutId);
	CharLayout getChar(FontLayoutId, char16_t, uint16_t &face);
	StringView getFontName(FontLayoutId);

	Rc<renderqueue::DependencyEvent> addTextureChars(FontLayoutId, SpanView<CharSpec>);

	uint32_t getFamilyIndex(StringView) const;
	StringView getFamilyName(uint32_t idx) const;

	void update();

protected:
	friend class FontLibrary;

	void setImage(Rc<gl::DynamicImage> &&);
	void setLoaded(bool);

	class FontSizedLayout;
	class FontLayout;

	FontLayout * getFontLayout(const FontParameters &style);

	void setAliases(Map<String, String> &&);

	bool _loaded = false;
	String _defaultFontFamily = "default";
	Rc<Texture> _texture;
	Rc<gl::DynamicImage> _image;
	Rc<FontLibrary> _library;

	Map<String, String> _aliases;
	Vector<StringView> _familiesNames;
	Map<StringView, Vector<FontLayout *>> _families;
	HashMap<StringView, Rc<FontLayout>> _layouts;
	Vector<FontSizedLayout *> _sizes;
	Rc<renderqueue::DependencyEvent> _dependency;

	std::atomic<uint16_t> _nextId = 1;
	bool _dirty = false;
	mutable Mutex _mutex;
};

class FontLibrary : public Ref {
public:
	enum class DefaultFontName {
		None,
		RobotoMono_Bold,
		RobotoMono_BoldItalic,
		RobotoMono_Italic,
		RobotoMono_Regular,
	};

	struct FontData {
		bool persistent;
		BytesView view;
		Bytes bytes;
		Function<Bytes()> callback;

		FontData(BytesView v, bool p) : persistent(p) {
			if (persistent) {
				view = v;
			} else {
				bytes = v.bytes<Interface>();
				view = bytes;
			}
		}
		FontData(Bytes &&b) : persistent(false), bytes(move(b)) {
			view = bytes;
		}
		FontData(Function<Bytes()> &&cb) : persistent(true), callback(move(cb)) { }
	};

	static BytesView getFont(DefaultFontName);
	static StringView getFontName(DefaultFontName);

	FontLibrary();
	virtual ~FontLibrary();

	bool init(const Rc<gl::Loop> &);

	const Application *getApplication() const { return _application; }

	Rc<FontFaceData> openFontData(StringView, const Callback<FontData()> & = nullptr);

	Rc<FontFaceObject> openFontFace(StringView, FontSize, const Callback<FontData()> &);
	Rc<FontFaceObject> openFontFace(const Rc<FontFaceData> &, FontSize);

	void update();

	FontController::Builder makeDefaultControllerBuilder(StringView);

	Rc<FontController> acquireController(FontController::Builder &&);

	void updateImage(const Rc<gl::DynamicImage> &, Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&,
			Rc<renderqueue::DependencyEvent> &&);

protected:
	FT_Face newFontFace(BytesView);
	void doneFontFace(FT_Face);

	void onActivated();

	struct ImageQuery {
		Rc<gl::DynamicImage> image;
		Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> chars;
		Rc<renderqueue::DependencyEvent> dependency;
	};

	bool _active = false;

	Mutex _mutex;
	uint16_t _nextId = 0;
	Map<StringView, Rc<FontFaceObject>> _faces;
	Map<StringView, Rc<FontFaceData>> _data;
	FT_Library _library = nullptr;

	const Application *_application = nullptr;
	Rc<gl::Loop> _loop; // for texture streaming
	Rc<renderqueue::Queue> _queue;
	Vector<ImageQuery> _pendingImageQueries;
};

}

#endif /* XENOLITH_FEATURES_FONT_XLFONTLIBRARY_H_ */
