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

#include "XLDefine.h"
#include "XLFontStyle.h"

namespace stappler::xenolith::font {

constexpr uint32_t LAYOUT_PADDING  = 1;

String getFontConfigName(const StringView &fontFamily, FontSize fontSize, FontStyle fontStyle, FontWeight fontWeight,
		FontStretch fontStretch, FontVariant fontVariant, bool caps) {
	auto size = fontSize;
	String name;
	name.reserve(fontFamily.size() + 14);
	name += fontFamily.str<Interface>();

	if (caps && fontVariant == FontVariant::SmallCaps) {
		size -= FontSize(size.get() / 5);
	}
	if (size.get() == 0) {
		if (size == FontSize::XXSmall) {
			name += ".xxs";
		} else if (size == FontSize::XSmall) {
			name += ".xs";
		} else if (size == FontSize::Small) {
			name += ".s";
		} else if (size == FontSize::Medium) {
			name += ".m";
		} else if (size == FontSize::Large) {
			name += ".l";
		} else if (size == FontSize::XLarge) {
			name += ".xl";
		} else if (size == FontSize::XXLarge) {
			name += ".xxl";
		} else {
			name += "." + toString(size.get());
		}
	}

	switch (fontStyle) {
	case FontStyle::Normal: name += ".n"; break;
	case FontStyle::Italic: name += ".i"; break;
	case FontStyle::Oblique: name += ".o"; break;
	}

	switch (fontWeight) {
	case FontWeight::Normal: name += ".n"; break;
	case FontWeight::Bold: name += ".b"; break;
	case FontWeight::W100: name += ".100"; break;
	case FontWeight::W200: name += ".200"; break;
	case FontWeight::W300: name += ".300"; break;
	case FontWeight::W500: name += ".500"; break;
	case FontWeight::W600: name += ".600"; break;
	case FontWeight::W800: name += ".800"; break;
	case FontWeight::W900: name += ".900"; break;
	}

	switch (fontStretch) {
	case FontStretch::Normal: name += ".n"; break;
	case FontStretch::UltraCondensed: name += ".ucd"; break;
	case FontStretch::ExtraCondensed: name += ".ecd"; break;
	case FontStretch::Condensed: name += ".cd"; break;
	case FontStretch::SemiCondensed: name += ".scd"; break;
	case FontStretch::SemiExpanded: name += ".sex"; break;
	case FontStretch::Expanded: name += ".ex"; break;
	case FontStretch::ExtraExpanded: name += ".eex"; break;
	case FontStretch::UltraExpanded: name += ".uex"; break;
	}

	return name;
}

FontParameters FontParameters::create(const String &str) {
	FontParameters ret;

	enum State {
		Family,
		Size,
		Style,
		Weight,
		Stretch,
		Overflow,
	} state = Family;

	string::split(str, ".", [&] (const StringView &ir) {
		StringView r(ir);
		switch (state) {
		case Family:
			ret.fontFamily = r.str<Interface>();
			state = Size;
			break;
		case Size:
			if (r.is("xxs")) { ret.fontSize = FontSize::XXSmall; }
			else if (r.is("xs")) { ret.fontSize = FontSize::XSmall; }
			else if (r.is("s")) { ret.fontSize = FontSize::Small; }
			else if (r.is("m")) { ret.fontSize = FontSize::Medium; }
			else if (r.is("l")) {ret.fontSize = FontSize::Large; }
			else if (r.is("xl")) { ret.fontSize = FontSize::XLarge; }
			else if (r.is("xxl")) { ret.fontSize = FontSize::XXLarge; }
			else { r.readInteger().unwrap([&] (int64_t value) { ret.fontSize = FontSize(value); }); }
			state = Style;
			break;
		case Style:
			if (r.is("i")) { ret.fontStyle = FontStyle::Italic; }
			else if (r.is("o")) { ret.fontStyle = FontStyle::Oblique; }
			else if (r.is("n")) { ret.fontStyle = FontStyle::Normal; }
			state = Weight;
			break;
		case Weight:
			if (r.is("n")) { ret.fontWeight = FontWeight::Normal; }
			else if (r.is("b")) { ret.fontWeight = FontWeight::Bold; }
			else if (r.is("100")) { ret.fontWeight = FontWeight::W100; }
			else if (r.is("200")) { ret.fontWeight = FontWeight::W200; }
			else if (r.is("300")) { ret.fontWeight = FontWeight::W300; }
			else if (r.is("400")) { ret.fontWeight = FontWeight::W400; }
			else if (r.is("500")) { ret.fontWeight = FontWeight::W500; }
			else if (r.is("600")) { ret.fontWeight = FontWeight::W600; }
			else if (r.is("700")) { ret.fontWeight = FontWeight::W700; }
			else if (r.is("800")) { ret.fontWeight = FontWeight::W800; }
			else if (r.is("900")) { ret.fontWeight = FontWeight::W900; }
			state = Stretch;
			break;
		case Stretch:
			if (r.is("n")) { ret.fontStretch = FontStretch::Normal; }
			else if (r.is("ucd")) { ret.fontStretch = FontStretch::UltraCondensed; }
			else if (r.is("ecd")) { ret.fontStretch = FontStretch::ExtraCondensed; }
			else if (r.is("cd")) { ret.fontStretch = FontStretch::Condensed; }
			else if (r.is("scd")) { ret.fontStretch = FontStretch::SemiCondensed; }
			else if (r.is("sex")) { ret.fontStretch = FontStretch::SemiExpanded; }
			else if (r.is("ex")) { ret.fontStretch = FontStretch::Expanded; }
			else if (r.is("eex")) { ret.fontStretch = FontStretch::ExtraExpanded; }
			else if (r.is("uex")) { ret.fontStretch = FontStretch::UltraExpanded; }
			state = Overflow;
			break;
		default: break;
		}
	});
	return ret;
}

String FontParameters::getConfigName(bool caps) const {
	return getFontConfigName(fontFamily, fontSize, fontStyle, fontWeight, fontStretch, fontVariant, caps);
}

FontParameters FontParameters::getSmallCaps() const {
	FontParameters ret = *this;
	ret.fontSize -= FontSize(ret.fontSize.get() / 5);
	return ret;
}

void FontCharString::addChar(char16_t c) {
	auto it = std::lower_bound(chars.begin(), chars.end(), c);
	if (it == chars.end() || *it != c) {
		chars.insert(it, c);
	}
}

void FontCharString::addString(const String &str) {
	addString(string::toUtf16<Interface>(str));
}

void FontCharString::addString(const WideString &str) {
	addString(str.data(), str.size());
}

void FontCharString::addString(const char16_t *str, size_t len) {
	for (size_t i = 0; i < len; ++ i) {
		const char16_t &c = str[i];
		auto it = std::lower_bound(chars.begin(), chars.end(), c);
		if (it == chars.end() || *it != c) {
			chars.insert(it, c);
		}
	}
}

void FontCharString::addString(const FontCharString &str) {
	addString(str.chars.data(), str.chars.size());
}

struct LayoutNodeMemory;

struct LayoutNodeMemoryStorage {
	const EmplaceCharInterface *interface = nullptr;
	LayoutNodeMemory *free = nullptr;
	memory::pool_t *pool = nullptr;

	LayoutNodeMemoryStorage(const EmplaceCharInterface *, memory::pool_t *);

	LayoutNodeMemory *alloc(const URect &rect);
	LayoutNodeMemory *alloc(const UVec2 &origin, void *c);
	void release(LayoutNodeMemory *node);
};

struct LayoutNodeMemory : memory::AllocPool {
	LayoutNodeMemory* _child[2];
	URect _rc;
	void * _char = nullptr;

	LayoutNodeMemory(const URect &rect);
	LayoutNodeMemory(const LayoutNodeMemoryStorage &, const UVec2 &origin, void *c);
	~LayoutNodeMemory();

	bool insert(LayoutNodeMemoryStorage &, void *c);
	size_t nodes() const;
	void finalize(LayoutNodeMemoryStorage &, uint8_t tex);
};

LayoutNodeMemoryStorage::LayoutNodeMemoryStorage(const EmplaceCharInterface *i, memory::pool_t *p)
: interface(i), pool(p) { }

LayoutNodeMemory *LayoutNodeMemoryStorage::alloc(const URect &rect) {
	if (free) {
		auto node = free;
		free = node->_child[0];
		return new (node) LayoutNodeMemory(rect);
	}
	return new (pool) LayoutNodeMemory(rect);
}

LayoutNodeMemory *LayoutNodeMemoryStorage::alloc(const UVec2 &origin, void *c) {
	if (free) {
		auto node = free;
		free = node->_child[0];
		return new (node) LayoutNodeMemory(*this, origin, c);
	}
	return new (pool) LayoutNodeMemory(*this, origin, c);
}

void LayoutNodeMemoryStorage::release(LayoutNodeMemory *node) {
	node->_child[0] = nullptr;
	node->_child[1] = nullptr;
	node->_rc = URect({0, 0, 0, 0});
	node->_char = nullptr;
	if (free) {
		node->_child[0] = free;
	}
	free = node;
}

LayoutNodeMemory::LayoutNodeMemory(const URect &rect) {
	_rc = rect;
	_child[0] = nullptr;
	_child[1] = nullptr;
}

LayoutNodeMemory::LayoutNodeMemory(const LayoutNodeMemoryStorage &storage, const UVec2 &origin, void *c) {
	_child[0] = nullptr;
	_child[1] = nullptr;
	_char = c;
	_rc = URect{origin.x, origin.y, storage.interface->getWidth(c), storage.interface->getHeight(c)};
}

LayoutNodeMemory::~LayoutNodeMemory() { }

bool LayoutNodeMemory::insert(LayoutNodeMemoryStorage &storage, void *c) {
	if (_child[0] && _child[1]) {
		return _child[0]->insert(storage, c) || _child[1]->insert(storage, c);
	} else {
		if (_char) {
			return false;
		}

		const auto iwidth = storage.interface->getWidth(c);
		const auto iheight = storage.interface->getHeight(c);

		if (_rc.width < iwidth || _rc.height < iheight) {
			return false;
		}

		if (_rc.width == iwidth || _rc.height == iheight) {
			if (_rc.height == iheight) {
				_child[0] = storage.alloc(_rc.origin(), c); // new (pool) LayoutNode(_rc.origin(), c);
				_child[1] = storage.alloc(URect{uint32_t(_rc.x + iwidth + LAYOUT_PADDING), _rc.y,
					(_rc.width > iwidth + LAYOUT_PADDING)?uint32_t(_rc.width - iwidth - LAYOUT_PADDING):uint32_t(0), _rc.height});
			} else if (_rc.width == iwidth) {
				_child[0] = storage.alloc(_rc.origin(), c); // new (pool) LayoutNode(_rc.origin(), c);
				_child[1] = storage.alloc(URect{_rc.x, uint32_t(_rc.y + iheight + LAYOUT_PADDING),
					_rc.width, (_rc.height > iheight + LAYOUT_PADDING)?uint32_t(_rc.height - iheight - LAYOUT_PADDING):uint32_t(0)});
			}
			return true;
		}

		//(decide which way to split)
		int16_t dw = _rc.width - iwidth;
		int16_t dh = _rc.height - iheight;

		if (dw > dh) {
			_child[0] = storage.alloc(URect{_rc.x, _rc.y, iwidth, _rc.height});
			_child[1] = storage.alloc(URect{uint32_t(_rc.x + iwidth + LAYOUT_PADDING), _rc.y, uint32_t(dw - LAYOUT_PADDING), _rc.height});
		} else {
			_child[0] = storage.alloc(URect{_rc.x, _rc.y, _rc.width, iheight});
			_child[1] = storage.alloc(URect{_rc.x, uint32_t(_rc.y + iheight + LAYOUT_PADDING), _rc.width, uint32_t(dh - LAYOUT_PADDING)});
		}

		_child[0]->insert(storage, c);

		return true;
	}
}

size_t LayoutNodeMemory::nodes() const {
	if (_char) {
		return 1;
	} else if (_child[0] && _child[1]) {
		return _child[0]->nodes() + _child[1]->nodes();
	} else {
		return 0;
	}
}

void LayoutNodeMemory::finalize(LayoutNodeMemoryStorage &storage, uint8_t tex) {
	if (_char) {
		storage.interface->setX(_char, _rc.x);
		storage.interface->setY(_char, _rc.y);
		storage.interface->setTex(_char, tex);
	} else {
		if (_child[0]) { _child[0]->finalize(storage, tex); }
		if (_child[1]) { _child[1]->finalize(storage, tex); }
	}
	 storage.release(this);
}

Extent2 emplaceChars(const EmplaceCharInterface &iface, const SpanView<void *> &layoutData, float totalSquare) {
	if (std::isnan(totalSquare)) {
		totalSquare = 0.0f;
		for (auto &it : layoutData) {
			totalSquare += iface.getWidth(it) * iface.getHeight(it);
		}
	}

	// find potential best match square
	bool s = true;
	uint32_t h = 128, w = 128; uint32_t sq2 = h * w;
	for (; sq2 < totalSquare; sq2 *= 2, s = !s) {
		if (s) { w *= 2; } else { h *= 2; }
	}

	LayoutNodeMemoryStorage storage(&iface, memory::pool::acquire());

	while (true) {
		auto l = storage.alloc(URect{0, 0, w, h});
		for (auto &it : layoutData) {
			if (!l->insert(storage, it)) {
				break;
			}
		}

		auto nodes = l->nodes();
		if (nodes == layoutData.size()) {
			l->finalize(storage, 0);
			storage.release(l);
			break;
		} else {
			if (s) { w *= 2; } else { h *= 2; }
			sq2 *= 2; s = !s;
		}
		storage.release(l);
	}

	return Extent2(w, h);
}

uint32_t CharLayout::getObjectId(uint16_t sourceId, char16_t ch, FontAnchor a) {
	uint32_t ret = ch;
	ret |= (toInt(a) << (sizeof(char16_t) * 8));
	ret |= (sourceId << ((sizeof(char16_t) * 8) + 2));
	return ret;
}

uint32_t CharLayout::getObjectId(uint32_t ret, FontAnchor a) {
	return (ret & (~ (3 << (sizeof(char16_t) * 8)))) | (toInt(a) << (sizeof(char16_t) * 8));
}

FontAnchor CharLayout::getAnchorForObject(uint32_t obj) {
	obj >>= sizeof(char16_t) * 8;
	obj &= 0b11;
	return FontAnchor(obj);
}

}
