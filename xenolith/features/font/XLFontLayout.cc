/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLFontLayout.h"
#include "XLFontLibrary.h"

namespace stappler::xenolith::font {

String FontLayout::constructName(StringView family, const FontSpecializationVector &vec) {
	return getFontConfigName(family, vec.fontSize, vec.fontStyle, vec.fontWeight, vec.fontStretch, vec.fontGrade, FontVariant::Normal, false);
}

bool FontLayout::init(String &&name, StringView family, FontSpecializationVector &&spec, Rc<FontFaceData> &&data, FontLibrary *c) {
	_name = move(name);
	_family = family.str<Interface>();
	_spec = move(spec);
	_sources.emplace_back(move(data));
	_faces.resize(_sources.size(), nullptr);
	_library = c;
	if (auto face = _library->openFontFace(_sources.front(), _spec)) {
		_faces[0] = face;
		_metrics = _faces.front()->getMetrics();
	}
	return true;
}

bool FontLayout::init(String &&name, StringView family, FontSpecializationVector &&spec, Vector<Rc<FontFaceData>> &&data, FontLibrary *c) {
	_name = move(name);
	_family = family.str<Interface>();
	_spec = move(spec);
	_sources = move(data);
	_faces.resize(_sources.size(), nullptr);
	_library = c;
	if (auto face = _library->openFontFace(_sources.front(), _spec)) {
		_faces[0] = face;
		_metrics = _faces.front()->getMetrics();
	}
	return true;
}

void FontLayout::touch(uint64_t clock, bool persistent) {
	_accessTime = clock;
	_persistent = persistent;
}

bool FontLayout::addString(const FontCharString &str, Vector<char16_t> &failed) {
	std::shared_lock lock(_mutex);

	bool shouldOpenFonts = false;
	bool updated = false;
	size_t i = 0;

	for (auto &it : _faces) {
		if (i == 0) {
			if (it->addChars(str.chars, i == 0, &failed)) {
				updated = true;
			}
		} else {
			if (it == nullptr) {
				shouldOpenFonts = true;
				break;
			}

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

	if (shouldOpenFonts) {
		lock.unlock();
		std::unique_lock lock(_mutex);

		for (; i < _faces.size(); ++ i) {
			if (_faces[i] == nullptr) {
				_faces[i] = _library->openFontFace(_sources[i], _spec);
			}

			auto tmp = move(failed);
			failed.clear();

			if (_faces[i]->addChars(tmp, i == 0, &failed)) {
				updated = true;
			}

			if (failed.empty()) {
				break;
			}
		}
	}

	return updated;
}

uint16_t FontLayout::getFontHeight() const {
	return _metrics.height;
}

int16_t FontLayout::getKerningAmount(char16_t first, char16_t second, uint16_t face) const {
	std::shared_lock lock(_mutex);
	for (auto &it : _faces) {
		if (it) {
			if (it->getId() == face) {
				return it->getKerningAmount(first, second);
			}
		} else {
			return 0;
		}
	}
	return 0;
}

Metrics FontLayout::getMetrics() const {
	return _metrics;
}

CharLayout FontLayout::getChar(char16_t ch, uint16_t &face) const {
	std::shared_lock lock(_mutex);
	uint16_t i = 0;
	for (auto &it : _faces) {
		auto l = it->getChar(ch);
		if (l.charID != 0) {
			face = it->getId();
			return l;
		}
		++ i;
	}
	return CharLayout();
}

bool FontLayout::addTextureChars(SpanView<CharSpec> chars) const {
	std::shared_lock lock(_mutex);

	bool ret = false;
	for (auto &it : chars) {
		if (chars::isspace(it.charID) || it.charID == char16_t(0x0A) || it.charID == char16_t(0x00AD)) {
			continue;
		}

		for (auto &f : _faces) {
			if (f && f->getId() == it.face) {
				if (f->addRequiredChar(it.charID)) {
					ret = true;
					break;
				}
			}
		}
	}
	return ret;
}

const Vector<Rc<FontFaceObject>> &FontLayout::getFaces() const {
	return _faces;
}

size_t FontLayout::getFaceCount() const {
	return _sources.size();
}

/*void FontController::FontLayout::addData(Rc<FontFaceData> &&data, bool front, Vector<FontSizedLayout *> &sizes) {
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
}*/

Rc<FontFaceData> FontLayout::getSource(size_t idx) const {
	if (idx < _sources.size()) {
		return _sources[idx];
	}
	return nullptr;
}

/*void FontController::FontLayout::updateSizedFaces(size_t prefix, Vector<FontSizedLayout *> &sizes) {
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
}*/

/*Rc<FontSizedLayout> FontController::FontLayout::getSizedFace(FontSize size, std::atomic<uint16_t> &nextId) {
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
}*/

}
