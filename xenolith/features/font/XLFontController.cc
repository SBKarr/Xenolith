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

#include "XLFontController.h"
#include "XLFontLibrary.h"
#include "XLFontLayout.h"

namespace stappler::xenolith::font {

XL_DECLARE_EVENT_CLASS(FontController, onLoaded)
XL_DECLARE_EVENT_CLASS(FontController, onFontSourceUpdated)

struct FontController::Builder::Data {
	String name;
	Map<String, FontSource> dataQueries;
	Map<String, FamilyQuery> familyQueries;
	Map<String, String> aliases;
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

const FontController::FontSource * FontController::Builder::addFontSource(StringView name, BytesView data, FontLayoutParameters params) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontExternalData = data;
		it->second.params = params;
		return &it->second;
	}

	log::vtext("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource * FontController::Builder::addFontSource(StringView name, Bytes && data, FontLayoutParameters params) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontMemoryData = move(data);
		it->second.params = params;
		return &it->second;
	}

	log::vtext("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource * FontController::Builder::addFontSource(StringView name, FilePath data, FontLayoutParameters params) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontFilePath = data.get().str<Interface>();
		it->second.params = params;
		return &it->second;
	}

	log::vtext("FontController", "Duplicate font source: ", name);
	return nullptr;
}

const FontController::FontSource * FontController::Builder::addFontSource(StringView name, Function<Bytes()> &&cb, FontLayoutParameters params) {
	auto it = _data->dataQueries.find(name);
	if (it == _data->dataQueries.end()) {
		it = _data->dataQueries.emplace(name.str<Interface>(), FontSource()).first;
		it->second.fontCallback = move(cb);
		it->second.params = params;
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

const FontController::FamilyQuery * FontController::Builder::addFontFaceQuery(StringView family, const FontSource *source, bool front) {
	XL_ASSERT(source, "Source should not be nullptr");

	auto it = _data->familyQueries.find(family);
	if (it == _data->familyQueries.end()) {
		it = _data->familyQueries.emplace(family.str<Interface>(), FamilyQuery{family.str<Interface>()}).first;
	}

	addSources(&it->second, Vector<const FontSource *>{source}, front);
	return &it->second;
}

const FontController::FamilyQuery * FontController::Builder::addFontFaceQuery(StringView family, Vector<const FontSource *> &&sources, bool front) {
	auto it = _data->familyQueries.find(family);
	if (it == _data->familyQueries.end()) {
		it = _data->familyQueries.emplace(family.str<Interface>(), FamilyQuery{family.str<Interface>()}).first;
	}

	addSources(&it->second, move(sources), front);
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

FontController::~FontController() {
	// image need to be finalized to remove cycled refs
	_image->finalize();
}

bool FontController::init(const Rc<FontLibrary> &lib) {
	_library = lib;
	return true;
}

void FontController::addFont(StringView family, Rc<FontFaceData> &&data, bool front) {
	std::unique_lock lock(_layoutSharedMutex);
	auto familyIt = _families.find(family);
	if (familyIt == _families.end()) {
		familyIt = _families.emplace(family.str<Interface>(), FamilySpec()).first;
		_familiesNames.emplace_back(familyIt->first);
	}

	if (!familyIt->second.data.empty() && front) {
		familyIt->second.data.emplace(familyIt->second.data.begin(), move(data));
	} else {
		familyIt->second.data.emplace_back(move(data));
	}

	_dirty = true;
	lock.unlock();

	if (_loaded) {
		onFontSourceUpdated(this);
	}
}

void FontController::addFont(StringView family, Vector<Rc<FontFaceData>> &&data, bool front) {
	std::unique_lock lock(_layoutSharedMutex);
	auto familyIt = _families.find(family);
	if (familyIt == _families.end()) {
		familyIt = _families.emplace(family.str<Interface>(), FamilySpec()).first;
		_familiesNames.emplace_back(familyIt->first);
	}

	if (familyIt->second.data.empty()) {
		familyIt->second.data = move(data);
	} else {
		if (front) {
			for (auto &it : data) {
				familyIt->second.data.emplace(familyIt->second.data.begin(), move(it));
			}
		} else {
			for (auto &it : data) {
				familyIt->second.data.emplace_back(move(it));
			}
		}
	}

	_dirty = true;
	lock.unlock();

	if (_loaded) {
		onFontSourceUpdated(this);
	}
}

bool FontController::addAlias(StringView newAlias, StringView familyName) {
	std::unique_lock lock(_layoutSharedMutex);
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

Rc<FontLayout> FontController::getLayout(const FontParameters &style) {
	Rc<FontLayout> ret;

	FontLayout *face = nullptr;
	auto family = style.fontFamily;

	// check if layout already loaded
	std::shared_lock sharedLock(_layoutSharedMutex);
	if (!_loaded) {
		return nullptr;
	}

	if (family.empty()) {
		family = StringView(_defaultFontFamily);
	}

	auto a_it = _aliases.find(family);
	if (a_it != _aliases.end()) {
		family = a_it->second;
	}

	auto familyIt = _families.find(family);
	if (familyIt == _families.end()) {
		return nullptr;
	}

	// search for exact match
	auto cfgName = FontLayout::constructName(family, style);
	auto it = _layouts.find(cfgName);
	if (it != _layouts.end()) {
		face = it->second.get();
	}

	FontSpecializationVector spec;
	if (!face) {
		// find best possible config
		spec = findSpecialization(familyIt->second, style, nullptr);
		cfgName = FontLayout::constructName(family, spec);
		auto it = _layouts.find(cfgName);
		if (it != _layouts.end()) {
			face = it->second.get();
		}
	}

	if (face) {
		face->touch(_clock, style.persistent);
		return face;
	}

	// we need to create new layout
	sharedLock.unlock();
	std::unique_lock uniqueLock(_layoutSharedMutex);

	Vector<Rc<FontFaceData>> data;

	// update best match (if fonts was updated)
	spec = findSpecialization(familyIt->second, style, &data);
	cfgName = FontLayout::constructName(family, spec);

	// check if somebody already created layout for us in another thread
	it = _layouts.find(cfgName);
	if (it != _layouts.end()) {
		it->second->touch(_clock, style.persistent);
		return it->second.get();
	}

	// create layout
	ret = Rc<FontLayout>::create(move(cfgName), family, move(spec), move(data), _library);
	_layouts.emplace(ret->getName(), ret);
	ret->touch(_clock, style.persistent);
	return ret;
}

Rc<FontLayout> FontController::getLayoutForString(const FontParameters &f, const FontCharString &str) {
	if (auto l = getLayout(f)) {
		Vector<char16_t> failed;
		if (f.persistent) {
			l->addString(str, failed);
		} else {
			l->addString(str, failed);
		}
		return l;
	}
	return nullptr;
}

Rc<renderqueue::DependencyEvent> FontController::addTextureChars(const Rc<FontLayout> &l, SpanView<CharSpec> chars) {
	if (l->addTextureChars(chars)) {
		if (!_dependency) {
			_dependency = Rc<renderqueue::DependencyEvent>::alloc();
		}
		_dirty = true;
		return _dependency;
	}
	return nullptr;
}

uint32_t FontController::getFamilyIndex(StringView name) const {
	std::shared_lock lock(_layoutSharedMutex);
	auto it = std::find(_familiesNames.begin(), _familiesNames.end(), name);
	if (it != _familiesNames.end()) {
		return it - _familiesNames.begin();
	}
	return maxOf<uint32_t>();
}

StringView FontController::getFamilyName(uint32_t idx) const {
	std::shared_lock lock(_layoutSharedMutex);
	if (idx < _familiesNames.size()) {
		return _familiesNames[idx];
	}
	return StringView();
}

void FontController::update(uint64_t clock) {
	_clock = clock;
	removeUnusedLayouts();
	if (_dirty && _loaded) {
		Vector<Pair<Rc<FontFaceObject>, Vector<char16_t>>> objects;
		std::shared_lock lock(_layoutSharedMutex);
		for (auto &it : _layouts) {
			for (auto &iit : it.second->getFaces()) {
				if (!iit) {
					continue;
				}

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
		update(platform::device::_clock(platform::device::ClockType::Monotonic));
	}
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

FontSpecializationVector FontController::findSpecialization(const FamilySpec &family, const FontParameters &params, Vector<Rc<FontFaceData>> *dataList) {
	auto getFontFaceScore = [] (const FontLayoutParameters &required, const FontLayoutParameters &existed) -> uint32_t {
		uint32_t ret = 0;
		// if no match - prefer normal variants
		if (existed.fontStyle == FontStyle::Normal) { ret += 50; }
		if (existed.fontWeight == FontWeight::Normal) { ret += 50; }
		if (existed.fontStretch == FontStretch::Normal) { ret += 50; }

		if ((required.fontStyle == FontStyle::Italic && existed.fontStyle == FontStyle::Italic)
				|| (required.fontStyle == FontStyle::Normal && existed.fontStyle == FontStyle::Normal)) {
			ret += 100000;
		} else if (existed.fontStyle == FontStyle::Italic) {
			if (required.fontStyle != FontStyle::Normal) {
				ret += ((360 << 6) - std::abs(int(required.fontStyle.get()) - int(FontStyle::Oblique.get()))) / 2;
			}
		} else if (required.fontStyle == FontStyle::Italic) {
			if (existed.fontStyle != FontStyle::Normal) {
				ret += ((360 << 6) - std::abs(int(FontStyle::Oblique.get()) - int(existed.fontStyle.get()))) / 2;
			}
		} else {
			ret += (360 << 6) - std::abs(int(required.fontStyle.get()) - int(existed.fontStyle.get()));
		}

		if (existed.fontStyle == required.fontStyle && (existed.fontStyle == FontStyle::Oblique || existed.fontStyle  == FontStyle::Italic)) {

		} else if ((existed.fontStyle == FontStyle::Oblique || existed.fontStyle == FontStyle::Italic)
				&& (required.fontStyle == FontStyle::Oblique || required.fontStyle == FontStyle::Italic)) {
			ret += 75000; // Oblique-Italic replacement
		} else if (existed.fontStyle == required.fontStyle && existed.fontStyle == FontStyle::Normal) {
			ret += 50000;
		}

		ret += (1000 - std::abs(int(required.fontWeight.get()) - int(existed.fontWeight.get()))) * 100;
		ret += ((250 << 1) - std::abs(int(required.fontStretch.get()) - int(existed.fontStretch.get()))) * 100;
		return ret;
	};

	uint32_t score = 0;
	FontSpecializationVector ret;

	Vector<Pair<FontFaceData *, uint32_t>> scores;

	uint32_t offset = family.data.size();
	for (auto &it : family.data) {
		auto spec = it->getSpecialization(params);
		auto specScore = getFontFaceScore(params, spec) + offset;
		if (dataList) {
			scores.emplace_back(pair(it.get(), specScore));
		}
		if (specScore >= score) {
			score = specScore;
			ret = spec;
		}
		-- offset;
	}

	if (dataList) {
		std::sort(scores.begin(), scores.end(), [] (const Pair<FontFaceData *, uint32_t> &l, const Pair<FontFaceData *, uint32_t> &r) {
			if (l.second == r.second) {
				return l.first < r.first;
			} else {
				return l.second > r.second;
			}
		});

		dataList->reserve(scores.size());
		for (auto &it : scores) {
			dataList->emplace_back(it.first);
		}
	}

	return ret;
}

void FontController::removeUnusedLayouts() {
	std::unique_lock lock(_layoutSharedMutex);
	auto it = _layouts.begin();
	while (it != _layouts.end()) {
		if (it->second->isPersistent()) {
			++ it;
			continue;
		}

		if (/*_clock - it->second->getAccessTime() > _unusedInterval.toMicros() &&*/ it->second->getReferenceCount() == 1) {
			it = _layouts.erase(it);
			_dirty = true;
		} else {
			++ it;
		}
	}
}

}
