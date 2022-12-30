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

#ifndef XENOLITH_FEATURES_FONT_XLFONTCONTROLLER_H_
#define XENOLITH_FEATURES_FONT_XLFONTCONTROLLER_H_

#include "XLResourceCache.h"
#include "XLFontFace.h"
#include "XLEventHeader.h"

#include <shared_mutex>

namespace stappler::xenolith::font {

class FontFaceObject;
class FontFaceData;
class FontLibrary;
class FontLayout;

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
		FontLayoutParameters params;
	};

	struct FamilyQuery {
		String family;
		Vector<const FontSource *> sources;
	};

	struct FamilySpec {
		Vector<Rc<FontFaceData>> data;
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

		const FontSource * addFontSource(StringView name, BytesView data, FontLayoutParameters = FontLayoutParameters());
		const FontSource * addFontSource(StringView name, Bytes && data, FontLayoutParameters = FontLayoutParameters());
		const FontSource * addFontSource(StringView name, FilePath data, FontLayoutParameters = FontLayoutParameters());
		const FontSource * addFontSource(StringView name, Function<Bytes()> &&cb, FontLayoutParameters = FontLayoutParameters());

		const FontSource *getFontSource(StringView) const;

		const FamilyQuery * addFontFaceQuery(StringView family, const FontSource *, bool front = false);
		const FamilyQuery * addFontFaceQuery(StringView family, Vector<const FontSource *> &&, bool front = false);

		bool addAlias(StringView newAlias, StringView familyName);

		Vector<const FamilyQuery *> getFontFamily(StringView family) const;
		Map<String, String> getAliases() const;

		Data *getData() const { return _data; }

	protected:
		void addSources(FamilyQuery *, Vector<const FontSource *> &&, bool front);

		Data *_data;
	};

	virtual ~FontController();

	bool init(const Rc<FontLibrary> &);

	void addFont(StringView family, Rc<FontFaceData> &&, bool front = false);
	void addFont(StringView family, Vector<Rc<FontFaceData>> &&, bool front = false);

	// replaces previous alias
	bool addAlias(StringView newAlias, StringView familyName);

	bool isLoaded() const { return _loaded; }
	const Rc<gl::DynamicImage> &getImage() const { return _image; }
	const Rc<Texture> &getTexture() const { return _texture; }

	Rc<FontLayout> getLayout(const FontParameters &f);
	Rc<FontLayout> getLayoutForString(const FontParameters &f, const FontCharString &);

	Rc<renderqueue::DependencyEvent> addTextureChars(const Rc<FontLayout> &, SpanView<CharSpec>);

	uint32_t getFamilyIndex(StringView) const;
	StringView getFamilyName(uint32_t idx) const;

	void update(uint64_t clock);

protected:
	friend class FontLibrary;

	void setImage(Rc<gl::DynamicImage> &&);
	void setLoaded(bool);

	// FontLayout * getFontLayout(const FontParameters &style);

	void setAliases(Map<String, String> &&);

	FontSpecializationVector findSpecialization(const FamilySpec &, const FontParameters &, Vector<Rc<FontFaceData>> *);
	void removeUnusedLayouts();

	bool _loaded = false;
	std::atomic<uint64_t> _clock;
	TimeInterval _unusedInterval = 100_msec;
	String _defaultFontFamily = "default";
	Rc<Texture> _texture;
	Rc<gl::DynamicImage> _image;
	Rc<FontLibrary> _library;

	Map<String, String> _aliases;
	Vector<StringView> _familiesNames;
	Map<String, FamilySpec> _families;
	HashMap<StringView, Rc<FontLayout>> _layouts;
	Rc<renderqueue::DependencyEvent> _dependency;

	bool _dirty = false;
	mutable std::shared_mutex _layoutSharedMutex;
};

}

#endif /* XENOLITH_FEATURES_FONT_XLFONTCONTROLLER_H_ */
