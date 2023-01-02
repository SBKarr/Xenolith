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

#include "MaterialStyleContainer.h"
#include "XLScene.h"

namespace stappler::xenolith::material {

XL_DECLARE_EVENT_CLASS(StyleContainer, onAttached)
XL_DECLARE_EVENT_CLASS(StyleContainer, onPrimaryColorSchemeUpdate)
XL_DECLARE_EVENT_CLASS(StyleContainer, onExtraColorSchemeUpdate)

bool StyleContainer::init() {
	if (!Component::init()) {
		return false;
	}

	return true;
}

void StyleContainer::onEnter(Scene *scene) {
	Component::onEnter(scene);
	_scene = scene;
	_scene->setFrameUserdata(this);
	onAttached(this, true);
}

void StyleContainer::onExit() {
	onAttached(this, false);
	if (_scene->getFrameUserdata() == this) {
		_scene->setFrameUserdata(nullptr);
	}
	_scene = nullptr;
	Component::onExit();
}

void StyleContainer::setPrimaryScheme(ThemeType type, const CorePalette &palette) {
	_primaryScheme.set(type, palette);
	if (_running) {
		onPrimaryColorSchemeUpdate(this);
	}
}

void StyleContainer::setPrimaryScheme(ThemeType type, const Color4F &color, bool isContent) {
	_primaryScheme.set(type, color, isContent);
	if (_running) {
		onPrimaryColorSchemeUpdate(this);
	}
}

void StyleContainer::setPrimaryScheme(ThemeType type, const ColorHCT &color, bool isContent) {
	_primaryScheme.set(type, color, isContent);
	if (_running) {
		onPrimaryColorSchemeUpdate(this);
	}
}

const ColorScheme &StyleContainer::getPrimaryScheme() const {
	return _primaryScheme;
}

const ColorScheme *StyleContainer::setExtraScheme(StringView name, ThemeType type, const CorePalette &palette) {
	auto it = _extraSchemes.find(name);
	if (it != _extraSchemes.end()) {
		it->second.set(type, palette);
	} else {
		it = _extraSchemes.emplace(name.str<Interface>(), ColorScheme(type, palette)).first;
	}
	if (_running) {
		onExtraColorSchemeUpdate(this, name);
	}
	return &it->second;
}

const ColorScheme *StyleContainer::setExtraScheme(StringView name, ThemeType type, const Color4F &color, bool isContent) {
	auto it = _extraSchemes.find(name);
	if (it != _extraSchemes.end()) {
		it->second.set(type, color, isContent);
	} else {
		it = _extraSchemes.emplace(name.str<Interface>(), ColorScheme(type, color, isContent)).first;
	}
	if (_running) {
		onExtraColorSchemeUpdate(this, name);
	}
	return &it->second;
}

const ColorScheme *StyleContainer::setExtraScheme(StringView name, ThemeType type, const ColorHCT &color, bool isContent) {
	auto it = _extraSchemes.find(name);
	if (it != _extraSchemes.end()) {
		it->second.set(type, color, isContent);
	} else {
		it = _extraSchemes.emplace(name.str<Interface>(), ColorScheme(type, color, isContent)).first;
	}
	if (_running) {
		onExtraColorSchemeUpdate(this, name);
	}
	return &it->second;
}

const ColorScheme *StyleContainer::getExtraScheme(StringView name) const {
	auto it = _extraSchemes.find(name);
	if (it != _extraSchemes.end()) {
		return &it->second;
	}
	return nullptr;
}

}
