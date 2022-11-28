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

#include "XLDefaultFont-RobotoMono-Bold.cc"
#include "XLDefaultFont-RobotoMono-BoldItalic.cc"
#include "XLDefaultFont-RobotoMono-Italic.cc"
#include "XLDefaultFont-RobotoMono-Regular.cc"

namespace stappler::xenolith::font {

XL_DECLARE_EVENT_CLASS(FontController, onLoaded)
XL_DECLARE_EVENT_CLASS(FontController, onFontSourceUpdated)

struct FontController::Builder::Data {
	String name;
	Map<String, FontSource> dataQueries;
	Map<String, FamilyQuery> familyQueries;
	Map<String, String> aliases;
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

	void addData(Rc<FontFaceData> &&, bool front, Vector<FontSizedLayout *> &sizes);
	void addData(Vector<Rc<FontFaceData>> &&, bool front, Vector<FontSizedLayout *> &sizes);

	Rc<FontSizedLayout> getSizedFace(FontSize, std::atomic<uint16_t> &);

	FontSizedLayout *openExtraSources(FontSizedLayout *, Vector<char16_t> &);

	Rc<FontFaceData> getSource(size_t) const;
	FontLibrary *getLibrary() const { return _library; }

protected:
	void updateSizedFaces(size_t prefix, Vector<FontSizedLayout *> &sizes);

	String _name;
	String _family;
	FontStyle _fontStyle = FontStyle::Normal;
	FontWeight _fontWeight = FontWeight::Normal;
	FontStretch _fontStretch = FontStretch::Normal;
	Vector<Rc<FontFaceData>> _sources;
	HashMap<FontSize, Rc<FontSizedLayout>> _faces;
	FontLibrary *_library = nullptr;
};

FontController::Builder::~Builder() {
	if (_data) {
		delete _data;
		_data = nullptr;
	}
}

FontController::Builder::Builder(StringView name) {
	_data = new Data();
	_data->name = name.str<Interface>();
}

FontController::Builder::Builder(Builder &&other) {
	_data = other._data;
	other._data = nullptr;
}

FontController::Builder &FontController::Builder::operator=(Builder &&other) {
	if (_data) {
		delete _data;
		_data = nullptr;
	}

	_data = other._data;
	other._data = nullptr;
	return *this;
}

StringView FontController::Builder::getName() const {
	return _data->name;
}

const FontController::FontSource * FontController::Builder::addFontSource(StringView name, BytesView data) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontExternalData = data;
		return &it->second;
	}

	log::vtext("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource * FontController::Builder::addFontSource(StringView name, Bytes && data) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontMemoryData = move(data);
		return &it->second;
	}

	log::vtext("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource * FontController::Builder::addFontSource(StringView name, FilePath data) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontFilePath = data.get().str<Interface>();
		return &it->second;
	}

	log::vtext("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource * FontController::Builder::addFontSource(StringView name, Function<Bytes()> &&cb) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontCallback = move(cb);
		return &it->second;
	}

	log::vtext("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource *FontController::Builder::getFontSource(StringView name) const {
	auto it = _data->dataQueries.find(name);
	if (it != _data->dataQueries.end()) {
		return &it->second;
	}
	return nullptr;
}

const FontController::FamilyQuery * FontController::Builder::addFontFaceQuery(StringView family,
		FontStyle style, FontWeight weight, FontStretch stretch, const FontSource *source,
		Vector<Pair<FontSize,FontCharString>> &&chars, bool front) {
	XL_ASSERT(source, "Source should not be nullptr");

	auto name = FontLayout::constructName(family, style, weight, stretch);
	auto it = _data->familyQueries.find(name);
	if (it == _data->familyQueries.end()) {
		it = _data->familyQueries.emplace(name, FamilyQuery{family.str<Interface>(),
			style, weight, stretch}).first;
	}

	addSources(&it->second, Vector<const FontSource *>{source}, front);
	if (!chars.empty()) {
		addChars(&it->second, move(chars));
	}

	return &it->second;
}

const FontController::FamilyQuery * FontController::Builder::addFontFaceQuery(StringView family,
		FontStyle style, FontWeight weight, FontStretch stretch, Vector<const FontSource *> &&sources,
		Vector<Pair<FontSize,FontCharString>> &&chars, bool front) {
	auto name = FontLayout::constructName(family, style, weight, stretch);
	auto it = _data->familyQueries.find(name);
	if (it == _data->familyQueries.end()) {
		it = _data->familyQueries.emplace(name, FamilyQuery{family.str<Interface>(),
			style, weight, stretch}).first;
	}

	addSources(&it->second, move(sources), front);
	if (!chars.empty()) {
		addChars(&it->second, move(chars));
	}

	return &it->second;
}

bool FontController::Builder::addAlias(StringView newAlias, StringView familyName) {
	auto iit = _data->aliases.find(familyName);
	if (iit != _data->aliases.end()) {
		_data->aliases.insert_or_assign(newAlias.str<Interface>(), iit->second);
		return true;
	} else {
		// check if family defined
		for (auto &it : _data->familyQueries) {
			if (it.second.family == familyName) {
				_data->aliases.insert_or_assign(newAlias.str<Interface>(), it.second.family);
				return true;
			}
		}
		return false;
	}
}

Vector<const FontController::FamilyQuery *> FontController::Builder::getFontFamily(StringView family) const {
	Vector<const FontController::FamilyQuery *> families;
	for (auto &it : _data->familyQueries) {
		if (it.second.family == family) {
			families.emplace_back(&it.second);
		}
	}
	return families;
}

Map<String, String> FontController::Builder::getAliases() const {
	return _data->aliases;
}

void FontController::Builder::addSources(FamilyQuery *query, Vector<const FontSource *> &&sources, bool front) {
	if (query->sources.empty() || !front) {
		query->sources.reserve(query->sources.size() + sources.size());
		for (auto &iit : sources) {
			XL_ASSERT(iit, "Source should not be nullptr");
			if (std::find(query->sources.begin(), query->sources.end(), iit) == query->sources.end()) {
				query->sources.emplace_back(iit);
			}
		}
	} else {
		auto iit = query->sources.begin();
		while (iit != query->sources.end()) {
			if (std::find(sources.begin(), sources.end(), *iit) != sources.end()) {
				iit = query->sources.erase(iit);
			} else {
				++ iit;
			}
		}

		query->sources.reserve(query->sources.size() + sources.size());

		auto insertIt = query->sources.begin();
		for (auto &iit : sources) {
			XL_ASSERT(iit, "Source should not be nullptr");
			if (std::find(query->sources.begin(), query->sources.end(), iit) == query->sources.end()) {
				query->sources.emplace(insertIt, iit);
			}
		}
	}
}

void FontController::Builder::addChars(FamilyQuery *query, Vector<Pair<FontSize, FontCharString>> &&chars) {
	if (query->chars.empty()) {
		query->chars = move(chars);
	} else {
		for (auto &it : chars) {
			auto iit = std::find_if(query->chars.begin(), query->chars.end(), [&] (const Pair<FontSize, FontCharString> &data) {
				if (data.first == it.first) {
					return true;
				}
				return false;
			});
			if (iit != query->chars.end()) {
				iit->second.addString(it.second);
			} else {
				query->chars.emplace_back(move(it));
			}
		}
	}
}

bool FontSizedLayout::init(FontSize size, String &&name, FontLayoutId id, FontController::FontLayout *l, Rc<FontFaceObject> &&root) {
	_size = size;
	_name = move(name);
	_id = id;
	_layout = l;
	_faces.emplace_back(move(root));
	_metrics = _faces.front()->getMetrics();
	return true;
}

bool FontSizedLayout::init(FontSize size, String &&name, FontLayoutId id, FontController::FontLayout *l, Vector<Rc<FontFaceObject>> &&faces) {
	_size = size;
	_name = move(name);
	_id = id;
	_layout = l;
	_faces = move(faces);
	_metrics = _faces.front()->getMetrics();
	return true;
}

bool FontSizedLayout::isComplete() const {
	return _faces.size() == _layout->getFaceCount();
}

bool FontSizedLayout::addString(const FontCharString &str, Vector<char16_t> &failed) const {
	bool updated = false;
	size_t i = 0;

	for (auto &it : _faces) {
		if (i == 0) {
			if (it->addChars(str.chars, i == 0, &failed)) {
				updated = true;
			}
		} else {
			auto tmp = move(failed);
			failed.clear();

			if (it->addChars(tmp, i == 0, &failed)) {
				updated = true;
			}
		}

		if (failed.empty()) {
			break;
		}

		++ i;
	}

	/*while (i < count) {
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
	}*/
	return updated;
}

uint16_t FontSizedLayout::getFontHeight() const {
	return _metrics.height;
}

int16_t FontSizedLayout::getKerningAmount(char16_t first, char16_t second, uint16_t face) const {
	for (auto &it : _faces) {
		if (it->getId() == face) {
			return it->getKerningAmount(first, second);
		}
	}
	return 0;
}

Metrics FontSizedLayout::getMetrics() const {
	return _metrics;
}

CharLayout FontSizedLayout::getChar(char16_t ch, uint16_t &face) const {
	for (auto &it : _faces) {
		auto l = it->getChar(ch);
		if (l.charID != 0) {
			face = it->getId();
			return l;
		}
	}
	return CharLayout();
}

StringView FontSizedLayout::getFontName() const {
	return _name;
}

bool FontSizedLayout::addTextureChars(SpanView<CharSpec> chars) const {
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

void FontController::FontLayout::addData(Rc<FontFaceData> &&data, bool front, Vector<FontSizedLayout *> &sizes) {
	if (front) {
		_sources.emplace(_sources.begin(), move(data));
		updateSizedFaces(1, sizes);
	} else {
		_sources.emplace_back(move(data));
	}
}

void FontController::FontLayout::addData(Vector<Rc<FontFaceData>> &&data, bool front, Vector<FontSizedLayout *> &sizes) {
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
			updateSizedFaces(count, sizes);
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

void FontController::FontLayout::updateSizedFaces(size_t prefix, Vector<FontSizedLayout *> &sizes) {
	for (auto &it : _faces) {
		auto faces = it.second->getFaces();
		auto id = it.second->getId();

		for (size_t i = 0; i < prefix; ++ i) {
			auto face = _library->openFontFace(getSource(i), it.second->getSize());
			faces.emplace(faces.begin(), face);
		}

		auto newLayout = Rc<FontSizedLayout>::create(it.second->getSize(), it.second->getName().str<Interface>(), id, this, move(faces));

		sizes[id.get()] = newLayout.get();
		it.second = newLayout;
	}
}

Rc<FontSizedLayout> FontController::FontLayout::getSizedFace(FontSize size, std::atomic<uint16_t> &nextId) {
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

FontSizedLayout *FontController::FontLayout::openExtraSources(FontSizedLayout *l, Vector<char16_t> &failed) {
	auto tmp = Rc<FontSizedLayout>(l);
	auto faces = tmp->getFaces();

	auto sourcesOffset = faces.size();
	auto count = _sources.size();
	if (sourcesOffset >= count) {
		return nullptr;
	}

	auto it = _faces.find(tmp->getSize());
	if (it == _faces.end()) {
		return nullptr;
	}

	while (sourcesOffset < count) {
		faces.emplace_back(_library->openFontFace(getSource(sourcesOffset), tmp->getSize()));

		auto tmp = move(failed);
		failed.clear();

		faces[sourcesOffset]->addChars(tmp, false, &failed);

		if (failed.empty()) {
			break;
		}

		++ sourcesOffset;
	}

	it->second = Rc<FontSizedLayout>::create(tmp->getSize(), tmp->getName().str<Interface>(), tmp->getId(), this, move(faces));
	return it->second.get();
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
		if (front) {
			std::unique_lock<Mutex> lock(_sizesMutex);
			it->second->addData(move(data), front, _sizes);
		} else {
			Vector<FontSizedLayout *> sizes;
			it->second->addData(move(data), front, sizes);
		}
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
		if (front) {
			// front addition can change sizes configuration
			std::unique_lock<Mutex> lock(_sizesMutex);
			it->second->addData(move(data), front, _sizes);
		} else {
			Vector<FontSizedLayout *> sizes;
			it->second->addData(move(data), front, sizes);
		}
	}

	_dirty = true;
	onFontSourceUpdated(this);
}

bool FontController::addAlias(StringView newAlias, StringView familyName) {
	if (_aliases.find(newAlias) != _aliases.end()) {
		return false;
	}

	auto iit = _aliases.find(familyName);
	if (iit != _aliases.end()) {
		_aliases.emplace(newAlias.str<Interface>(), iit->second);
		return true;
	} else {
		auto f_it = _families.find(familyName);
		if (f_it != _families.end()) {
			_aliases.emplace(newAlias.str<Interface>(), familyName.str<Interface>());
			return true;
		}
		return false;
	}
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
		std::unique_lock<Mutex> lock(_sizesMutex);
		if (ret.get() >= _sizes.size()) {
			_sizes.emplace_back(l.get());
		}
	} else {
		log::vtext("FontController", "Layout not found: ", style.getConfigName());
	}
	return ret;
}

void FontController::addString(FontLayoutId id, const FontCharString &str) {
	std::unique_lock<Mutex> lock(_sizesMutex);
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			Vector<char16_t> failed;
			l->addString(str, failed);
			if (!failed.empty() && !l->isComplete()) {
				if (auto newLayout = l->getLayout()->openExtraSources(l, failed)) {
					_sizes[id.get()] = newLayout;
				}
			}
		}
	}
}

uint16_t FontController::getFontHeight(FontLayoutId id) const {
	std::unique_lock<Mutex> lock(_sizesMutex);
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getFontHeight();
		}
	}
	return 0;
}

int16_t FontController::getKerningAmount(FontLayoutId id, char16_t first, char16_t second, uint16_t face) const {
	std::unique_lock<Mutex> lock(_sizesMutex);
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getKerningAmount(first, second, face);
		}
	}
	return 0;
}

Metrics FontController::getMetrics(FontLayoutId id) const {
	std::unique_lock<Mutex> lock(_sizesMutex);
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getMetrics();
		}
	}
	return Metrics();
}

CharLayout FontController::getChar(FontLayoutId id, char16_t ch, uint16_t &face) const {
	std::unique_lock<Mutex> lock(_sizesMutex);
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getChar(ch, face);
		}
	}
	return CharLayout();
}

StringView FontController::getFontName(FontLayoutId id) const {
	std::unique_lock<Mutex> lock(_sizesMutex);
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			return l->getFontName();
		}
	}
	return StringView();
}

Rc<FontSizedLayout> FontController::getSizedLayout(FontLayoutId id) const {
	std::unique_lock<Mutex> lock(_sizesMutex);
	if (id.get() < _sizes.size()) {
		return Rc<FontSizedLayout>(_sizes.at(id.get()));
	}
	return nullptr;
}

Rc<renderqueue::DependencyEvent> FontController::addTextureChars(FontLayoutId id, SpanView<CharSpec> chars) {
	std::unique_lock<Mutex> lock(_sizesMutex);
	if (id.get() < _sizes.size()) {
		auto l = _sizes.at(id.get());
		if (l) {
			if (l->addTextureChars(chars)) {
				if (!_dependency) {
					_dependency = Rc<renderqueue::DependencyEvent>::alloc();
				}
				_dirty = true;
				return _dependency;
			}
		}
	}
	return nullptr;
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
		std::unique_lock<Mutex> lock(_sizesMutex);
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
			_library->updateImage(_image, move(objects), move(_dependency));
			_dependency = nullptr;
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

	auto a_it = _aliases.find(family);
	if (a_it != _aliases.end()) {
		family = a_it->second;
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

void FontController::setAliases(Map<String, String> &&aliases) {
	if (_aliases.empty()) {
		_aliases = move(aliases);
	} else {
		for (auto &it : aliases) {
			_aliases.insert_or_assign(it.first, it.second);
		}
	}
}

BytesView FontLibrary::getFont(DefaultFontName name) {
	switch (name) {
	case DefaultFontName::None: return BytesView(); break;
	case DefaultFontName::RobotoMono_Bold: return BytesView(s_font_RobotoMono_Bold, 59235); break;
	case DefaultFontName::RobotoMono_BoldItalic: return BytesView(s_font_RobotoMono_BoldItalic, 65895); break;
	case DefaultFontName::RobotoMono_Italic: return BytesView(s_font_RobotoMono_Italic, 65086); break;
	case DefaultFontName::RobotoMono_Regular: return BytesView(s_font_RobotoMono_Regular, 58642); break;
	}

	return BytesView();
}

StringView FontLibrary::getFontName(DefaultFontName name) {
	switch (name) {
	case DefaultFontName::None: return StringView(); break;
	case DefaultFontName::RobotoMono_Bold: return StringView("RobotoMono_Bold"); break;
	case DefaultFontName::RobotoMono_BoldItalic: return StringView("RobotoMono_BoldItalic"); break;
	case DefaultFontName::RobotoMono_Italic: return StringView("RobotoMono_Italic"); break;
	case DefaultFontName::RobotoMono_Regular: return StringView("RobotoMono_Regular"); break;
	}

	return StringView();
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

bool FontLibrary::init(const Rc<gl::Loop> &loop) {
	_loop = loop;
	_queue = _loop->makeRenderFontQueue();
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

static Bytes openResourceFont(FontLibrary::DefaultFontName name) {
	auto d = FontLibrary::getFont(name);
	return data::decompress<memory::StandartInterface>(d.data(), d.size());
}

static String getResourceFontName(FontLibrary::DefaultFontName name) {
	return toString("resource:", FontLibrary::getFontName(name));
}

static const FontController::FontSource * makeResourceFontQuery(FontController::Builder &builder, FontLibrary::DefaultFontName name) {
	return builder.addFontSource( getResourceFontName(name), [name] { return openResourceFont(name); });
}

FontController::Builder FontLibrary::makeDefaultControllerBuilder(StringView key) {
	FontController::Builder ret(key);
	auto res_RobotoMono_Bold = makeResourceFontQuery(ret, DefaultFontName::RobotoMono_Bold);
	auto res_RobotoMono_BoldItalic = makeResourceFontQuery(ret, DefaultFontName::RobotoMono_BoldItalic);
	auto res_RobotoMono_Italic = makeResourceFontQuery(ret, DefaultFontName::RobotoMono_Italic);
	auto res_RobotoMono_Regular = makeResourceFontQuery(ret, DefaultFontName::RobotoMono_Regular);

	ret.addFontFaceQuery("monospace",
			font::FontStyle::Normal, font::FontWeight::Bold, font::FontStretch::Normal, res_RobotoMono_Bold);
	ret.addFontFaceQuery("monospace",
			font::FontStyle::Italic, font::FontWeight::Bold, font::FontStretch::Normal, res_RobotoMono_BoldItalic);
	ret.addFontFaceQuery("monospace",
			font::FontStyle::Italic, font::FontWeight::Normal, font::FontStretch::Normal, res_RobotoMono_Italic);
	ret.addFontFaceQuery("monospace",
			font::FontStyle::Normal, font::FontWeight::Normal, font::FontStretch::Normal, res_RobotoMono_Regular);

	ret.addAlias("default", "monospace");

	return ret;
}

Rc<FontController> FontLibrary::acquireController(FontController::Builder &&b) {
	Rc<FontController> ret = Rc<FontController>::create(this);

	struct ControllerBuilder : Ref {
		FontController::Builder builder;
		Rc<FontController> controller;
		Rc<gl::DynamicImage> dynamicImage;

		bool invalid = false;
		std::atomic<size_t> pendingData = 0;
		FontLibrary *library = nullptr;

		ControllerBuilder(FontController::Builder &&b)
		: builder(move(b)) { }

		void invalidate() {
			controller = nullptr;
		}

		void loadData() {
			if (invalid) {
				invalidate();
				return;
			}

			library->getApplication()->perform([this] (const thread::Task &) -> bool {
				for (auto &it : builder.getData()->familyQueries) {
					Vector<Rc<FontFaceData>> d; d.reserve(it.second.sources.size());
					for (auto &iit : it.second.sources) {
						d.emplace_back(move(iit->data));
					}

					controller->addFont(it.second.family, it.second.style, it.second.weight, it.second.stretch, move(d));
					for (auto &iit : it.second.chars) {
						if (!iit.second.empty()) {
							auto lId = controller->getLayout(FontParameters{
								it.second.style, it.second.weight, it.second.stretch,
								font::FontVariant::Normal,
								font::ListStyleType::None,
								iit.first, it.second.family
							}, 1.0f);
							controller->addString(lId, iit.second);
						}
					}
				}
				return true;
			}, [this] (const Task &, bool success) {
				if (success) {
					controller->setAliases(builder.getAliases());
					controller->setLoaded(true);
				}
				controller = nullptr;
			}, this);
		}

		void onDataLoaded(bool success) {
			auto v = pendingData.fetch_sub(1);
			if (!success) {
				invalid = true;
				if (v == 1) {
					invalidate();
				}
			} else if (v == 1) {
				loadData();
			}
		}

		void onImageLoaded(Rc<gl::DynamicImage> &&image) {
			auto v = pendingData.fetch_sub(1);
			if (image) {
				controller->setImage(move(image));
				if (v == 1) {
					loadData();
				}
			} else {
				invalid = true;
				if (v == 1) {
					invalidate();
				}
			}
		}
	};

	auto builder = Rc<ControllerBuilder>::alloc(move(b));
	builder->library = this;
	builder->controller = ret;

	builder->pendingData = builder->builder.getData()->dataQueries.size() + 1;

	for (auto &it : builder->builder.getData()->dataQueries) {
		_application->perform([this, name = it.first, sourcePtr = &it.second, builder] (const thread::Task &) -> bool {
			sourcePtr->data = openFontData(name, [&] () -> FontData {
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
			if (sourcePtr->data) {
				builder->onDataLoaded(true);
			} else {
				builder->onDataLoaded(false);
			}
			return true;
		});
	}

	builder->dynamicImage = Rc<gl::DynamicImage>::create([name = builder->builder.getName()] (gl::DynamicImage::Builder &builder) {
		builder.setImage(name,
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
	});

	_loop->compileImage(builder->dynamicImage, [this, builder] (bool success) {
		_loop->getApplication()->performOnMainThread([success, builder] {
			if (success) {
				builder->onImageLoaded(move(builder->dynamicImage));
			} else {
				builder->onImageLoaded(nullptr);
			}
		}, this);
	});

	return ret;
}

void FontLibrary::updateImage(const Rc<gl::DynamicImage> &image, Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> &&data,
		Rc<renderqueue::DependencyEvent> &&dep) {
	if (!_active) {
		_pendingImageQueries.emplace_back(ImageQuery{image, move(data), move(dep)});
		return;
	}

	auto input = Rc<gl::RenderFontInput>::alloc();
	input->image = image;
	input->requests = move(data);
	/*input->output = [] (const gl::ImageInfo &info, BytesView data) {
		Bitmap bmp(data, info.extent.width, info.extent.height, bitmap::PixelFormat::A8);
		bmp.save(bitmap::FileFormat::Png, toString(Time::now().toMicros(), ".png"));
	};*/

	auto req = Rc<renderqueue::FrameRequest>::create(_queue);
	if (dep) {
		req->addSignalDependency(move(dep));
	}

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
		updateImage(it.image, move(it.chars), move(it.dependency));
	}

	_pendingImageQueries.clear();
}

}
