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

#include "XLLabel.h"
#include "XLEventListener.h"

namespace stappler::xenolith {

Label::~Label() {
	_format = nullptr;
}

bool Label::init(font::FontController *source, const DescriptionStyle &style, const String &str, float width, Alignment alignment, float density) {
	if (!Sprite::init()) {
		return false;
	}

	if (density == 0.0f) {
		density = Application::getInstance()->getData().density;
	}

	_source = source;
	_style = style;
	_density = density;
	setNormalized(true);

	setColorMode(ColorMode::AlphaChannel);
	//setSurface(true);

	auto el = Rc<EventListener>::create();
	el->onEventWithObject(font::FontController::onFontSourceUpdated, source, std::bind(&Label::onFontSourceUpdated, this));

	if (_source->isLoaded()) {
		setTexture(Rc<Texture>(_source->getTexture()));
	} else {
		el->onEventWithObject(font::FontController::onLoaded, source, std::bind(&Label::onFontSourceLoaded, this), true);
	}

	addComponent(el);

	_listener = el;

	setColor(Color4F(_style.text.color, _style.text.opacity), true);

	// setNormalized(true);

	setString(str);
	setWidth(width);
	setAlignment(alignment);

	updateLabel();

	return true;
}

void Label::tryUpdateLabel(bool force) {
	if (_labelDirty) {
		updateLabel();
	}
}

void Label::setStyle(const DescriptionStyle &style) {
	_style = style;

	setColor(Color4F(_style.text.color, _style.text.opacity), true);

	_labelDirty = true;
}

const Label::DescriptionStyle &Label::getStyle() const {
	return _style;
}

void Label::updateLabel() {
	if (!_source) {
		return;
	}

	if (_string16.empty()) {
		_format = nullptr;
		_formatDirty = true;
		setContentSize(Size(0.0f, getFontHeight() / _density));
		return;
	}

	auto spec = Rc<layout::FormatSpec>::alloc(_source, _string16.size(), _compiledStyles.size() + 1);

	_compiledStyles = compileStyle();
	_style.text.color = _displayedColor.getColor();
	_style.text.opacity = _displayedColor.getOpacity();
	_style.text.whiteSpace = layout::style::WhiteSpace::PreWrap;

	updateFormatSpec(spec, _compiledStyles, _density, _adjustValue);

	_format = spec;

	if (_format) {
		if (_format->chars.empty()) {
			setContentSize(Size(0.0f, getFontHeight() / _density));
		} else {
			setContentSize(Size(_format->width / _density, _format->height / _density));
		}

		_labelDirty = false;
		_colorDirty = false;
		_formatDirty = true;
	}
}

void Label::visit(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (!_visible) {
		return;
	}
	if (_labelDirty) {
		updateLabel();
	}
	if (_formatDirty) {
		updateQuads();
	}
	if (_colorDirty) {
		updateColorQuads();
	}
	Node::visit(frame, parentFlags);
}

void Label::updateColor() {
	if (_format) {
		for (auto &it : _format->ranges) {
			if (!it.colorDirty) {
				it.color.r = uint8_t(_displayedColor.r / 255.0f);
				it.color.g = uint8_t(_displayedColor.g / 255.0f);
				it.color.b = uint8_t(_displayedColor.b / 255.0f);
			}
			if (!it.opacityDirty) {
				it.color.a = uint8_t(_displayedColor.a / 255.0f);
			}
		}
	}
	_colorDirty = true;
}

void Label::updateColorQuads() {
	if (!_colorMap.empty()) {
		_vertexes.updateColorQuads(_displayedColor, _colorMap);
	}
	_colorDirty = false;
}

static size_t Label_getQuadsCount(const layout::FormatSpec *format) {
	size_t ret = 0;

	const layout::RangeSpec *targetRange = nullptr;

	for (auto it = format->begin(); it != format->end(); ++ it) {
		if (&(*it.range) != targetRange) {
			targetRange = &(*it.range);
		}

		const auto start = it.start();
		auto end = start + it.count();
		if (it.line->start + it.line->count == end) {
			const layout::CharSpec &c = format->chars[end - 1];
			if (!string::isspace(c.charID) && c.charID != char16_t(0x0A)) {
				++ ret;
			}
			end -= 1;
		}

		for (auto charIdx = start; charIdx < end; ++ charIdx) {
			const layout::CharSpec &c = format->chars[charIdx];
			if (!string::isspace(c.charID) && c.charID != char16_t(0x0A) && c.charID != char16_t(0x00AD)) {
				++ ret;
			}
		}
	}

	return ret;
}

static void Label_writeTextureQuad(const layout::FormatSpec *format, const layout::Metrics &m, const layout::CharSpec &c,
		const font::FontCharLayout &l, const layout::RangeSpec &range, const layout::LineSpec &line, Vector<ColorMask> &cMap,
		uint16_t face, VertexArray::Quad &quad) {
	ColorMask mask = ColorMask::None;
	if (range.colorDirty) {
		mask |= ColorMask::Color;
	}
	if (range.opacityDirty) {
		mask |= ColorMask::A;
	}
	cMap.push_back(mask);

	switch (range.align) {
	case layout::VerticalAlign::Sub:
		quad.drawChar(m, l, c.pos, format->height - line.pos + m.descender / 2, range.color, range.decoration, face);
		break;
	case layout::VerticalAlign::Super:
		quad.drawChar(m, l, c.pos, format->height - line.pos + m.ascender / 2, range.color, range.decoration, face);
		break;
	default:
		quad.drawChar(m, l, c.pos, format->height - line.pos, range.color, range.decoration, face);
		break;
	}
}

void Label::updateQuadsForeground(font::FontController *controller, FormatSpec *format, Vector<ColorMask> &colorMap) {
	auto quadsCount = Label_getQuadsCount(format);
	colorMap.clear();
	colorMap.reserve(quadsCount);

	const layout::RangeSpec *targetRange = nullptr;
	layout::Metrics metrics;

	_vertexes.clear();

	for (auto it = format->begin(); it != format->end(); ++ it) {
		if (it.count() == 0) {
			continue;
		}

		if (&(*it.range) != targetRange) {
			targetRange = &(*it.range);
			metrics = format->source->getMetrics(targetRange->layout);
		}

		const auto start = it.start();
		auto end = start + it.count();

		for (auto charIdx = start; charIdx < end; ++ charIdx) {
			const layout::CharSpec &c = format->chars[charIdx];
			if (!string::isspace(c.charID) && c.charID != char16_t(0x0A) && c.charID != char16_t(0x00AD)) {

				uint16_t face = 0;
				auto ch = controller->getFullChar(targetRange->layout, c.charID, face);

				if (ch.layout.charID == c.charID) {
					auto quad = _vertexes.addQuad();
					Label_writeTextureQuad(format, metrics, c, ch, *it.range, *it.line, colorMap, face, quad);
				}
			}
		}

		if (it.line->start + it.line->count == end) {
			const layout::CharSpec &c = format->chars[end - 1];
			if (c.charID == char16_t(0x00AD)) {
				uint16_t face = 0;
				auto ch = controller->getFullChar(targetRange->layout, c.charID, face);

				if (ch.layout.charID == c.charID) {
					auto quad = _vertexes.addQuad();
					Label_writeTextureQuad(format, metrics, c, ch, *it.range, *it.line, colorMap, face, quad);
				}
			}
			end -= 1;
		}

		if (it.count() > 0 && it.range->decoration != layout::style::TextDecoration::None) {
			const layout::CharSpec &firstChar = format->chars[it.start()];
			const layout::CharSpec &lastChar = format->chars[it.start() + it.count() - 1];

			auto color = it.range->color;
			color.a = uint8_t(0.75f * color.a);
			auto metrics = format->source->getMetrics(it.range->layout);

			float offset = 0.0f;
			switch (it.range->decoration) {
			case layout::style::TextDecoration::None: break;
			case layout::style::TextDecoration::Overline:
				offset = metrics.height;
				break;
			case layout::style::TextDecoration::LineThrough:
				offset = metrics.height / 2.0f;
				break;
			case layout::style::TextDecoration::Underline:
				offset = metrics.height / 8.0f;
				break;
			}

			const float width = metrics.height / 16.0f;
			const float base = floorf(width);
			const float frac = width - base;

			/*quads[0]->drawRect(firstChar.pos, format->height - it.line->pos + offset, lastChar.pos + lastChar.advance - firstChar.pos, base,
					color, texs[0]->getPixelsWide(), texs[0]->getPixelsHigh());
			if (frac > 0.1) {
				color.a *= frac;
				quads[0]->drawRect(firstChar.pos, format->height - it.line->pos + offset - base + 1, lastChar.pos + lastChar.advance - firstChar.pos, 1,
						color, texs[0]->getPixelsWide(), texs[0]->getPixelsHigh());
			}*/
		}
	}

}

void Label::setStandalone(bool value) {
	if (_standalone != value) {
		_standalone = value;
		//_standaloneMap.clear();
		//_standaloneChars.clear();
		_formatDirty = true;
	}
}

bool Label::isStandalone() const {
	return _standalone;
}

void Label::setAdjustValue(uint8_t val) {
	if (_adjustValue != val) {
		_adjustValue = val;
		_labelDirty = true;
	}
}
uint8_t Label::getAdjustValue() const {
	return _adjustValue;
}

bool  Label::isOverflow() const {
	if (_format) {
		return _format->overflow;
	}
	return false;
}

size_t Label::getCharsCount() const {
	return _format?_format->chars.size():0;
}
size_t Label::getLinesCount() const {
	return _format?_format->lines.size():0;
}
Label::LineSpec Label::getLine(uint32_t num) const {
	if (_format) {
		if (num < _format->lines.size()) {
			return _format->lines[num];
		}
	}
	return LineSpec();
}

uint16_t Label::getFontHeight() const {
	auto l = _source->getLayout(_style.font, _density);
	if (l.get()) {
		return _source->getFontHeight(l);
	}
	return 0;
}

void Label::updateQuads() {
	if (!_source) {
		return;
	}

	if (!_format || _format->chars.size() == 0) {
		return;
	}

	if (!_standalone) {
		for (auto &it : _format->ranges) {
			_source->addTextureChars(it.layout, SpanView<layout::CharSpec>(_format->chars, it.start, it.count));
		}

		updateQuadsForeground(_source, _format, _colorMap);
		_formatDirty = false;
	}/* else {
		bool sourceDirty = false;
		for (auto &it : _format->ranges) {
			auto find_it = _standaloneChars.find(it.layout->getName());
			if (find_it == _standaloneChars.end()) {
				find_it = _standaloneChars.emplace(it.layout->getName(), Vector<char16_t>()).first;
			}

			auto &vec = find_it->second;
			for (uint32_t i = it.start; i < it.count; ++ i) {
				const char16_t &c = _format->chars[i].charID;
				auto char_it = std::lower_bound(vec.begin(), vec.end(), c);
				if (char_it == vec.end() || *char_it != c) {
					vec.emplace(char_it, c);
					sourceDirty = true;
				}
			}
		}

		if (sourceDirty) {
			_standaloneTextures.clear();
			_standaloneMap = _source->updateTextures(_standaloneChars, _standaloneTextures);
		}

		if (!_standaloneTextures.empty()) {
			updateQuadsStandalone(_source, _format);
		}
	}*/
}

void Label::onFontSourceUpdated() {
	if (!_standalone) {
		_formatDirty = true;
	}
}

void Label::onFontSourceLoaded() {
	if (_source) {
		setTexture(Rc<Texture>(_source->getTexture()));
		_formatDirty = true;
		_labelDirty = true;
	}
}

void Label::onLayoutUpdated() {
	_labelDirty = false;
}

Vec2 Label::getCursorPosition(uint32_t charIndex, bool front) const {
	if (_format) {
		if (charIndex < _format->chars.size()) {
			auto &c = _format->chars[charIndex];
			auto line = _format->getLine(charIndex);
			if (line) {
				return Vec2( (front ? c.pos : c.pos + c.advance) / _density, _contentSize.height - line->pos / _density);
			}
		} else if (charIndex >= _format->chars.size() && charIndex != 0) {
			auto &c = _format->chars.back();
			auto &l = _format->lines.back();
			if (c.charID == char16_t(0x0A)) {
				return getCursorOrigin();
			} else {
				return Vec2( (c.pos + c.advance) / _density, _contentSize.height - l.pos / _density);
			}
		}
	}

	return Vec2::ZERO;
}

Vec2 Label::getCursorOrigin() const {
	switch (_alignment) {
	case Alignment::Left:
	case Alignment::Justify:
		return Vec2( 0.0f / _density, _contentSize.height - _format->height / _density);
		break;
	case Alignment::Center:
		return Vec2( _contentSize.width * 0.5f / _density, _contentSize.height - _format->height / _density);
		break;
	case Alignment::Right:
		return Vec2( _contentSize.width / _density, _contentSize.height - _format->height / _density);
		break;
	}
	return Vec2::ZERO;
}

Pair<uint32_t, bool> Label::getCharIndex(const Vec2 &pos) const {
	auto ret = _format->getChar(pos.x * _density, _format->height - pos.y * _density, FormatSpec::Best);
	if (ret.first == maxOf<uint32_t>()) {
		return pair(maxOf<uint32_t>(), false);
	} else if (ret.second == FormatSpec::Prefix) {
		return pair(ret.first, false);
	} else {
		return pair(ret.first, true);
	}
}

float Label::getMaxLineX() const {
	if (_format) {
		return _format->maxLineX / _density;
	}
	return 0.0f;
}

}
