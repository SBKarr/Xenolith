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

#include "XLFontLibrary.h"
#include "XLFontFace.h"
#include "XLApplication.h"
#include "XLGlLoop.h"
#include "XLGlView.h"
#include "XLRenderQueueQueue.h"
#include "XLRenderQueueImageStorage.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace stappler::xenolith::font {

XL_DECLARE_EVENT_CLASS(FontController, onLoaded)
XL_DECLARE_EVENT_CLASS(FontController, onFontSourceUpdated)

class FontController::FontSizedLayout : public Ref {
public:
	virtual ~FontSizedLayout() { }
	FontSizedLayout() { }

	bool init(FontSize, String &&, FontLayoutId, FontLayout *, Rc<FontFaceObject> &&);

	FontSize getSize() const { return _size; }
	StringView getName() const { return _name; }
	FontLayoutId getId() const { return _id; }

	bool addString(const FontCharString &);
	uint16_t getFontHeight();
	int16_t getKerningAmount(char16_t first, char16_t second, uint16_t face) const;
	Metrics getMetrics();
	CharLayout getChar(char16_t, uint16_t &face);
	StringView getFontName();

	bool addTextureChars(SpanView<CharSpec>);

	const Vector<Rc<FontFaceObject>> &getFaces() const { return _faces; }

	void prefixFonts(size_t count);

protected:
	FontSize _size;
	String _name;
	FontLayoutId _id;
	FontLayout *_layout = nullptr;
	Metrics _metrics;
	Vector<Rc<FontFaceObject>> _faces;
};

class FontController::FontLayout : public Ref {
public:
	static String constructName(StringView, FontStyle, FontWeight, FontStretch);

	virtual ~FontLayout() { }
	FontLayout() { }

	bool init(String &&, StringView family, FontStyle style, FontWeight weight, FontStretch stretch,
			Rc<FontFaceData> &&data, FontLibrary *);
	bool init(String &&, StringView family, FontStyle style, FontWeight weight, FontStretch stretch,
			Vector<Rc<FontFaceData>> &&data, FontLibrary *);

	StringView getName() const { return _name; }
	StringView getFamily() const { return _family; }

	FontStyle getStyle() const { return _fontStyle; }
	FontWeight getWeight() const { return _fontWeight; }
	FontStretch getStretch() const { return _fontStretch; }

	size_t getFaceCount() const;

	void addData(Rc<FontFaceData> &&, bool front = false);
	void addData(Vector<Rc<FontFaceData>> &&, bool front = false);

	Rc<FontSizedLayout> getSizedFace(FontSize, std::atomic<uint16_t> &);

	Rc<FontFaceData> getSource(size_t) const;
	FontLibrary *getLibrary() const { return _library; }

protected:
	void updateSizedFaces(size_t prefix);

	String _name;
	String _family;
	FontStyle _fontStyle = FontStyle::Normal;
	FontWeight _fontWeight = FontWeight::Normal;
	FontStretch _fontStretch = FontStretch::Normal;
	Vector<Rc<FontFaceData>> _sources;
	HashMap<FontSize, Rc<FontSizedLayout>> _faces;
	FontLibrary *_library = nullptr;
};

bool FontController::FontSizedLayout::init(FontSize size, String &&name, FontLayoutId id, FontLayout *l, Rc<FontFaceObject> &&root) {
	_size = size;
	_name = move(name);
	_id = id;
	_layout = l;
	_faces.emplace_back(move(root));
	_metrics = _faces.front()->getMetrics();
	return true;
}

bool FontController::FontSizedLayout::addString(const FontCharString &str) {
	bool updated = false;
	size_t i = 0;
	size_t count = _layout->getFaceCount();

	Vector<char16_t> failed;

	while (i < count) {
		if (!_faces[i]) {
			_faces[i] = _layout->getLibrary()->openFontFace(_layout->getSource(i), _size);
		}

		if (i == 0) {
			if (_faces[i]->addChars(str.chars, i == 0, &failed)) {
				updated = true;
			}
		} else {
			auto tmp = move(failed);
			failed.clear();

			if (_faces[i]->addChars(tmp, i == 0, &failed)) {
				updated = true;
			}
		}

		if (failed.empty()) {
			break;
		}

		++ i;
	}
	return updated;
}

uint16_t FontController::FontSizedLayout::getFontHeight() {
	return _metrics.height;
}

int16_t FontController::FontSizedLayout::getKerningAmount(char16_t first, char16_t second, uint16_t face) const {
	for (auto &it : _faces) {
		if (it->getId() == face) {
			return it->getKerningAmount(first, second);
		}
	}
	return 0;
}

Metrics FontController::FontSizedLayout::getMetrics() {
	return _metrics;
}

CharLayout FontController::FontSizedLayout::getChar(char16_t ch, uint16_t &face) {
	for (auto &it : _faces) {
		auto l = it->getChar(ch);
		if (l.charID != 0) {
			face = it->getId();
			return l;
		}
	}
	return CharLayout();
}

StringView FontController::FontSizedLayout::getFontName() {
	return _name;
}

bool FontController::FontSizedLayout::addTextureChars(SpanView<CharSpec> chars) {
	bool ret = false;
	for (auto &it : chars) {
		if (chars::isspace(it.charID) || it.charID == char16_t(0x0A) || it.charID == char16_t(0x00AD)) {
			continue;
		}

		for (auto &f : _faces) {
			if (f->getId() == it.face) {
				if (f->addRequiredChar(it.charID)) {
					ret = true;
				}
			}
		}
	}
	return ret;
}

void FontController::FontSizedLayout::prefixFonts(size_t count) {
	for (size_t i = 0; i < count; ++ i) {
		auto face = _layout->getLibrary()->openFontFace(_layout->getSource(i), _size);
		_faces.emplace(_faces.begin(), face);
	}
}

String FontController::FontLayout::constructName(StringView family, FontStyle style, FontWeight weight, FontStretch stretch) {
	return getFontConfigName(family, FontSize(0), style, weight, stretch, FontVariant::Normal, false);
}

bool FontController::FontLayout::init(String &&name, StringView family, FontStyle style, FontWeight weight, FontStretch stretch,
		Rc<FontFaceData> &&data, FontLibrary *c) {
	_name = move(name);
	_family = family.str<Interface>();
	_fontStyle = style;
	_fontWeight = weight;
	_fontStretch = stretch;
	_sources.emplace_back(move(data));
	_library = c;
	return true;
}

bool FontController::FontLayout::init(String &&name, StringView family, FontStyle style, FontWeight weight, FontStretch stretch,
		Vector<Rc<FontFaceData>> &&data, FontLibrary *c) {
	_name = move(name);
	_family = family.str<Interface>();
	_fontStyle = style;
	_fontWeight = weight;
	_fontStretch = stretch;
	_sources = move(data);
	_library = c;
	return true;
}

size_t FontController::FontLayout::getFaceCount() const {
	return _sources.size();
}

void FontController::FontLayout::addData(Rc<FontFaceData> &&data, bool front) {
	if (front) {
		_sources.emplace(_sources.begin(), move(data));
		updateSizedFaces(1);
	} else {
		_sources.emplace_back(move(data));
	}
}

void FontController::FontLayout::addData(Vector<Rc<FontFaceData>> &&data, bool front) {
	if (_sources.empty()) {
		_sources = move(data);
	} else {
		_sources.reserve(_sources.size() + data.size());
		if (front) {
			size_t count = 0;
			auto target = _sources.begin();
			auto it = data.begin();
			while (it != data.end()) {
				target = _sources.emplace(target, move(*it));
				++ count;
			}
			updateSizedFaces(count);
		} else {
			for (auto &it : data) {
				_sources.emplace_back(move(it));
			}
		}
	}
}

Rc<FontFaceData> FontController::FontLayout::getSource(size_t idx) const {
	if (idx < _sources.size()) {
		return _sources[idx];
	}
	return nullptr;
}

void FontController::FontLayout::updateSizedFaces(size_t prefix) {
	for (auto &it : _faces) {
		it.second->prefixFonts(prefix);
	}
}

Rc<FontController::FontSizedLayout> FontController::FontLayout::getSizedFace(FontSize size, std::atomic<uint16_t> &nextId) {
	Rc<FontSizedLayout> ret;
	auto it = _faces.find(size);
	if (it != _faces.end()) {
		return it->second;
	}

	if (auto face = _library->openFontFace(_sources.front(), size)) {
		auto id = nextId.fetch_add(1);
		ret = Rc<FontSizedLayout>::create(size, toString(_name, "?size=", size.get()), FontLayoutId(id), this, move(face));
		_faces.emplace(size, ret);
	}

	return ret;
}

FontController::~FontController() {
	// image need to be finalized to remove cycled refs
	_image->finalize();
}

bool FontController::init(const Rc<FontLibrary> &lib) {
	_library = lib;
	_sizes.emplace_back(nullptr);
	return true;
}

void FontController::addFont(StringView family, FontStyle style, FontWeight weight, FontStretch stretch,
		Rc<FontFaceData> &&data, bool front) {
	auto name = FontLayout::constructName(family, style, weight, stretch);
	std::unique_lock<Mutex> lock(_mutex);
	auto it = _layouts.find(name);
	if (it != _layouts.end()) {
		auto l = Rc<FontLayout>::create(move(name), family, style, weight, stretch, move(data), _library);
		auto iit = _families.find(l->getFamily());
		if (iit != _families.end()) {
			iit->second.emplace_back(l.get());
		} else {
			_families.emplace(l->getFamily(), Vector<FontLayout *>{l.get()});
			_familiesNames.emplace_back(l->getFamily());
		}
		_layouts.emplace(l->getName(), move(l));
	} else {
		it->second->addData(move(data), front);
	}

	_dirty = true;
	onFontSourceUpdated(this);
}

void FontController::addFont(StringView family, FontStyle style, FontWeight weight, FontStretch stretch,
		Vector<Rc<FontFaceData>> &&data, bool front) {
	auto name = FontLayout::constructName(family, style, weight, stretch);
	std::unique_lock<Mutex> lock(_mutex);
	auto it = _layouts.find(name);
	if (it == _layouts.end()) {
		auto l = Rc<FontLayout>::create(move(name), family, style, weight, stretch, move(data), _library);
		auto iit = _families.find(l->getFamily());
		if (iit != _families.end()) {
			iit->second.emplace_back(l.get());
		} else {
			_families.emplace(l->getFamily(), Vector<FontLayout *>{l.get()});
			_familiesNames.emplace_back(l->getFamily());
		}
		_layouts.emplace(l->getName(), move(l));
	} else {
		it->second->addData(move(data), front);
	}

	_dirty = true;
	onFontSourceUpdated(this);
}

FontLayoutId FontController::getLayout(const FontParameters &style, float scale) {
	FontLayoutId ret = FontLayoutId(0);

	if (isnan(scale)) {
		scale = 1.0f;
	}

	std::unique_lock<Mutex> lock(_mutex);
	auto face = getFontLayout(style);
	if (face) {
		auto dsize = FontSize(roundf(style.fontSize.get() * scale));

		auto l = face->getSizedFace(dsize, _nextId);

		ret = l->getId();
		if (ret.get() >= _sizes.size()) {
			_sizes.emplace_back(l.get());
		}
	}
	return ret;
}

void FontController::addString(FontLayoutId id, const FontCharString &str) {
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			l->addString(str);
		}
	}
}

uint16_t FontController::getFontHeight(FontLayoutId id) {
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getFontHeight();
		}
	}
	return 0;
}

int16_t FontController::getKerningAmount(FontLayoutId id, char16_t first, char16_t second, uint16_t face) const {
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getKerningAmount(first, second, face);
		}
	}
	return 0;
}

Metrics FontController::getMetrics(FontLayoutId id) {
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getMetrics();
		}
	}
	return Metrics();
}

CharLayout FontController::getChar(FontLayoutId id, char16_t ch, uint16_t &face) {
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getChar(ch, face);
		}
	}
	return CharLayout();
}

StringView FontController::getFontName(FontLayoutId id) {
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getFontName();
		}
	}
	return StringView();
}

void FontController::addTextureChars(FontLayoutId id, SpanView<CharSpec> chars) {
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			if (l->addTextureChars(chars)) {
				_dirty = true;
			}
		}
	}
}

uint32_t FontController::getFamilyIndex(StringView name) const {
	std::unique_lock<Mutex> lock(_mutex);
	auto it = std::find(_familiesNames.begin(), _familiesNames.end(), name);
	if (it != _familiesNames.end()) {
		return it - _familiesNames.begin();
	}
	return maxOf<uint32_t>();
}

StringView FontController::getFamilyName(uint32_t idx) const {
	std::unique_lock<Mutex> lock(_mutex);
	if (idx < _familiesNames.size()) {
		return _familiesNames[idx];
	}
	return StringView();
}

void FontController::update() {
	if (_dirty && _loaded) {
		Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> objects;
		for (auto &it : _sizes) {
			if (!it) {
				continue;
			}
			for (auto &iit : it->getFaces()) {
				auto lb = std::lower_bound(objects.begin(), objects.end(), iit,
						[] (const Pair<Rc<FontFaceObject>, Vector<char16_t>> &l, FontFaceObject *r) {
					return l.first.get() < r;
				});
				if (lb == objects.end()) {
					auto req = iit->getRequiredChars();
					if (!req.empty()) {
						objects.emplace_back(iit, move(req));
					}
				} else if (lb != objects.end() && lb->first != iit) {
					auto req = iit->getRequiredChars();
					if (!req.empty()) {
						objects.emplace(lb, iit, move(req));
					}
				}
			}
		}
		if (!objects.empty()) {
			updateTexture(move(objects));
		}
		_dirty = false;
	}
}

void FontController::setImage(Rc<gl::DynamicImage> &&image) {
	_image = move(image);
	_texture = Rc<Texture>::create(_image);
}

void FontController::setLoaded(bool value) {
	if (_loaded != value) {
		_loaded = value;
		onLoaded(this);
		update();
	}
}

void FontController::updateTexture(Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&vec) {
	_library->updateImage(_image, move(vec));
}

FontController::FontLayout * FontController::getFontLayout(const FontParameters &style) {
	auto getFontFaceScore = [] (const FontParameters &params, const FontLayout &face) -> size_t {
		size_t ret = 0;
		if (face.getStyle() == FontStyle::Normal) {
			ret += 1;
		}
		if (face.getWeight() == FontWeight::Normal) {
			ret += 1;
		}
		if (face.getStretch() == FontStretch::Normal) {
			ret += 1;
		}

		if (face.getStyle() == params.fontStyle && (face.getStyle() == FontStyle::Oblique || face.getStyle() == FontStyle::Italic)) {
			ret += 100;
		} else if ((face.getStyle() == FontStyle::Oblique || face.getStyle() == FontStyle::Italic)
				&& (params.fontStyle == FontStyle::Oblique || params.fontStyle == FontStyle::Italic)) {
			ret += 75;
		}

		auto weightDiff = (int)toInt(FontWeight::W900) - abs((int)toInt(params.fontWeight) - (int)toInt(face.getWeight()));
		ret += weightDiff * 10;

		auto stretchDiff = (int)toInt(FontStretch::UltraExpanded) - abs((int)toInt(params.fontStretch) - (int)toInt(face.getStretch()));
		ret += stretchDiff * 5;

		return ret;
	};

	size_t score = 0;
	FontController::FontLayout *layout = nullptr;

	auto family = style.fontFamily;
	if (family.empty()) {
		family = StringView(_defaultFontFamily);
	}

	auto cfgName = FontLayout::constructName(family, style.fontStyle, style.fontWeight, style.fontStretch);

	auto it = _layouts.find(cfgName);
	if (it != _layouts.end()) {
		return it->second.get();
	}

	auto f_it = _families.find(family);
	if (f_it != _families.end()) {
		auto &faces = f_it->second;
		for (auto &face_it : faces) {
			auto newScore = getFontFaceScore(style, *face_it);
			if (newScore >= score) {
				score = newScore;
				layout = face_it;
			}
		}
	}

	if (!layout) {
		family = StringView(_defaultFontFamily);
		auto f_it = _families.find(family);
		if (f_it != _families.end()) {
			auto &faces = f_it->second;
			for (auto &face_it : faces) {
				auto newScore = getFontFaceScore(style, *face_it);
				if (newScore >= score) {
					score = newScore;
					layout = face_it;
				}
			}
		}
	}

	return layout;
}

FontLibrary::FontLibrary() {
	FT_Init_FreeType( &_library );
}

FontLibrary::~FontLibrary() {
	if (_library) {
		FT_Done_FreeType(_library);
		_library = nullptr;
	}
}

bool FontLibrary::init(const Rc<gl::Loop> &loop, Rc<renderqueue::Queue> &&queue) {
	_loop = loop;
	_queue = move(queue);
	_application = _loop->getApplication();
	if (_queue->isCompiled()) {
		onActivated();
	} else {
		auto linkId = retain();
		_loop->compileRenderQueue(_queue, [this, linkId] (bool success) {
			if (success) {
				_loop->getApplication()->performOnMainThread([this] {
					onActivated();
				}, this);
			}
			release(linkId);
		});
	}
	return true;
}

Rc<FontFaceData> FontLibrary::openFontData(StringView dataName, const Callback<FontData()> &dataCallback) {
	std::unique_lock<Mutex> lock(_mutex);

	auto it = _data.find(dataName);
	if (it != _data.end()) {
		return it->second;
	}

	if (!dataCallback) {
		return nullptr;
	}

	auto fontData = dataCallback();
	if (fontData.view.empty() && !fontData.callback) {
		return nullptr;
	}

	Rc<FontFaceData> dataObject;
	if (fontData.callback) {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.callback));
	} else if (fontData.persistent) {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.view), true);
	} else {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.bytes));
	}
	if (dataObject) {
		_data.emplace(dataObject->getName(), dataObject);
	}
	return dataObject;
}

Rc<FontFaceObject> FontLibrary::openFontFace(StringView dataName, FontSize size, const Callback<FontData()> &dataCallback) {
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
		auto ret = Rc<FontFaceObject>::create(faceName, it->second, face, size, _nextId);
		++ _nextId;
		if (ret) {
			_faces.emplace(ret->getName(), ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	if (!dataCallback) {
		return nullptr;
	}

	auto fontData = dataCallback();
	if (fontData.view.empty()) {
		return nullptr;
	}

	Rc<FontFaceData> dataObject;
	if (fontData.persistent) {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.view), true);
	} else {
		dataObject = Rc<FontFaceData>::create(dataName, move(fontData.bytes));
	}

	if (dataObject) {
		_data.emplace(dataObject->getName(), dataObject);
		auto face = newFontFace(dataObject->getView());
		auto ret = Rc<FontFaceObject>::create(faceName, it->second, face, size, _nextId);
		++ _nextId;
		if (ret) {
			_faces.emplace(ret->getName(), ret);
		} else {
			doneFontFace(face);
		}
		return ret;
	}

	return nullptr;
}

Rc<FontFaceObject> FontLibrary::openFontFace(const Rc<FontFaceData> &dataObject, FontSize size) {
	auto faceName = toString(dataObject->getName(), "?size=", size.get());

	std::unique_lock<Mutex> lock(_mutex);
	do {
		auto it = _faces.find(faceName);
		if (it != _faces.end()) {
			return it->second;
		}
	} while (0);

	auto face = newFontFace(dataObject->getView());
	auto ret = Rc<FontFaceObject>::create(faceName, dataObject, face, size, _nextId);
	++ _nextId;
	if (ret) {
		_faces.emplace(ret->getName(), ret);
	} else {
		doneFontFace(face);
	}
	return ret;
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

Rc<FontController> FontLibrary::acquireController(StringView key) {
	Rc<FontController> ret = Rc<FontController>::create(this);
	auto image = new Rc<gl::DynamicImage>( Rc<gl::DynamicImage>::create([&] (gl::DynamicImage::Builder &builder) {
		builder.setImage(key,
			gl::ImageInfo(
					Extent2(2, 2),
					gl::ImageUsage::Sampled | gl::ImageUsage::TransferSrc,
					gl::RenderPassType::Graphics,
					gl::ImageFormat::R8_UNORM
			), [] (const gl::ImageData::DataCallback &cb) {
				Bytes bytes; bytes.reserve(4);
				bytes.emplace_back(0);
				bytes.emplace_back(255);
				bytes.emplace_back(255);
				bytes.emplace_back(0);
				cb(bytes);
			}, nullptr);
		return true;
	}));
	_loop->compileImage(*image, [this, ret, image] (bool success) {
		_loop->getApplication()->performOnMainThread([this, ret, image, success] {
			if (success) {
				ret->setImage(move(*image));
				ret->setLoaded(true);
			}
			delete image;
		}, this);
	});
	return ret;
}

Rc<FontController> FontLibrary::acquireController(StringView key, FontController::Query &&query) {
	Rc<FontController> ret = Rc<FontController>::create(this);

	struct ControllerBuilder : Ref {
		FontController::Query query;
		Map<String, Pair<FontController::FontQuery *, Rc<FontFaceData>>> data;
		Rc<FontController> controller;

		bool invalid = false;
		bool imageLoaded = false;
		size_t pendingData = 0;
		FontLibrary *library = nullptr;

		void invalidate() {
			controller = nullptr;
		}

		void loadData() {
			if (invalid) {
				invalidate();
				return;
			}

			for (auto &it : query.familyQueries) {
				Vector<Rc<FontFaceData>> d; d.reserve(it.sources.size());
				for (auto &iit : it.sources) {
					auto vIt = data.find(iit);
					if (vIt != data.end()) {
						if (vIt->second.second) {
							d.emplace_back(vIt->second.second);
						}
					}
				}

				controller->addFont(it.family, it.style, it.weight, it.stretch, move(d));
				for (auto &iit : it.chars) {
					auto lId = controller->getLayout(FontParameters{
						it.style, it.weight, it.stretch,
						font::FontVariant::Normal,
						font::ListStyleType::None,
						iit.first, it.family
					}, 1.0f);
					controller->addString(lId, iit.second);
				}
			}

			controller->setLoaded(true);
			controller = nullptr;
		}

		void onDataLoaded(bool success) {
			-- pendingData;
			if (!success) {
				invalid = true;
				if (pendingData == 0 && imageLoaded) {
					invalidate();
				}
			} else if (pendingData == 0 && imageLoaded) {
				loadData();
			}
		}

		void onImageLoaded(Rc<gl::DynamicImage> &&image) {
			if (image) {
				controller->setImage(move(image));
				imageLoaded = true;
				if (pendingData == 0) {
					loadData();
				}
			} else {
				invalid = true;
				if (pendingData == 0) {
					invalidate();
				}
			}
		}
	};

	auto builder = Rc<ControllerBuilder>::alloc();
	builder->query = move(query);
	builder->library = this;
	builder->controller = ret;

	for (auto &it : builder->query.dataQueries) {
		builder->data.emplace(it.name, Pair<FontController::FontQuery *, Rc<FontFaceData>>(&it, nullptr));
	}

	builder->pendingData = builder->data.size();

	for (auto &it : builder->data) {
		_application->perform([this, sourcePtr = it.second.first, targetPtr = &it.second.second] (const thread::Task &) -> bool {
			*targetPtr = openFontData(sourcePtr->name, [&] () -> FontData {
				if (sourcePtr->fontCallback) {
					return FontData(move(sourcePtr->fontCallback));
				} else if (!sourcePtr->fontExternalData.empty()) {
					return FontData(sourcePtr->fontExternalData, true);
				} else if (!sourcePtr->fontMemoryData.empty()) {
					return FontData(move(sourcePtr->fontMemoryData));
				} else if (!sourcePtr->fontFilePath.empty()) {
					auto d = filesystem::readIntoMemory<Interface>(sourcePtr->fontFilePath);
					if (!d.empty()) {
						return FontData(move(d));
					}
				}
				return FontData(BytesView(), false);
			});
			return true;
		}, [builder] (const thread::Task &, bool success) {
			if (success) {
				builder->onDataLoaded(true);
			} else {
				builder->onDataLoaded(false);
			}
		});
	}

	auto image = new Rc<gl::DynamicImage>( Rc<gl::DynamicImage>::create([&] (gl::DynamicImage::Builder &builder) {
		builder.setImage(key,
			gl::ImageInfo(
					Extent2(2, 2),
					gl::ImageUsage::Sampled | gl::ImageUsage::TransferSrc,
					gl::RenderPassType::Graphics,
					gl::ImageFormat::R8_UNORM
			), [] (const gl::ImageData::DataCallback &cb) {
				Bytes bytes; bytes.reserve(4);
				bytes.emplace_back(0);
				bytes.emplace_back(255);
				bytes.emplace_back(255);
				bytes.emplace_back(0);
				cb(bytes);
			}, nullptr);
		return true;
	}));
	_loop->compileImage(*image, [this, image, builder] (bool success) {
		_loop->getApplication()->performOnMainThread([this, image, success, builder] {
			if (success) {
				builder->onImageLoaded(move(*image));
			} else {
				builder->onImageLoaded(nullptr);
			}
			delete image;
		}, this);
	});

	return ret;
}

void FontLibrary::updateImage(const Rc<gl::DynamicImage> &image, Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&data) {
	if (!_active) {
		_pendingImageQueries.emplace_back(ImageQuery{image, move(data)});
		return;
	}

	auto input = Rc<gl::RenderFontInput>::alloc();
	input->image = image;
	input->requests = move(data);
	/*input->output = [] (const gl::ImageInfo &info, BytesView data) {
		Bitmap bmp(data, info.extent.width, info.extent.height, Bitmap::PixelFormat::A8);
		bmp.save(Bitmap::FileFormat::Png, toString(Time::now().toMicros(), ".png"));
	};*/

	auto req = Rc<renderqueue::FrameRequest>::create(_queue);

	for (auto &it : _queue->getInputAttachments()) {
		req->addInput(it, move(input));
		break;
	}

	_loop->runRenderQueue(move(req));
}

FT_Face FontLibrary::newFontFace(BytesView data) {
	FT_Face ret = nullptr;
	FT_Error err = FT_Err_Ok;

	err = FT_New_Memory_Face(_library, data.data(), data.size(), 0, &ret );
	if (err != FT_Err_Ok) {
		auto str = FT_Error_String(err);
		log::text("font::FontLibrary", str ? StringView(str) : "Unknown error");
		return nullptr;
	}

	return ret;
}

void FontLibrary::doneFontFace(FT_Face face) {
	if (face) {
		FT_Done_Face(face);
	}
}

void FontLibrary::onActivated() {
	_active = true;

	for (auto &it : _pendingImageQueries) {
		updateImage(it.image, move(it.chars));
	}

	_pendingImageQueries.clear();
}

}
