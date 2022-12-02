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
#include "XLApplication.h"
#include "XLDeferredManager.h"

namespace stappler::xenolith {

static size_t Label_getQuadsCount(const font::FormatSpec *format) {
	size_t ret = 0;

	const font::RangeSpec *targetRange = nullptr;

	for (auto it = format->begin(); it != format->end(); ++ it) {
		if (&(*it.range) != targetRange) {
			targetRange = &(*it.range);
		}

		const auto start = it.start();
		auto end = start + it.count();
		if (it.line->start + it.line->count == end) {
			const font::CharSpec &c = format->chars[end - 1];
			if (!string::isspace(c.charID) && c.charID != char16_t(0x0A)) {
				++ ret;
			}
			end -= 1;
		}

		for (auto charIdx = start; charIdx < end; ++ charIdx) {
			const font::CharSpec &c = format->chars[charIdx];
			if (!string::isspace(c.charID) && c.charID != char16_t(0x0A) && c.charID != char16_t(0x00AD)) {
				++ ret;
			}
		}
	}

	return ret;
}

static void Label_pushColorMap(const font::RangeSpec &range, Vector<ColorMask> &cMap) {
	ColorMask mask = ColorMask::None;
	if (!range.colorDirty) {
		mask |= ColorMask::Color;
	}
	if (!range.opacityDirty) {
		mask |= ColorMask::A;
	}
	cMap.push_back(mask);
}

static void Label_writeTextureQuad(const font::FormatSpec *format, const font::Metrics &m, const font::CharSpec &c,
		const font::CharLayout &l, const font::RangeSpec &range, const font::LineSpec &line, VertexArray::Quad &quad) {
	switch (range.align) {
	case font::VerticalAlign::Sub:
		quad.drawChar(m, l, c.pos, format->height - line.pos + m.descender / 2, range.color, range.decoration, c.face);
		break;
	case font::VerticalAlign::Super:
		quad.drawChar(m, l, c.pos, format->height - line.pos + m.ascender / 2, range.color, range.decoration, c.face);
		break;
	default:
		quad.drawChar(m, l, c.pos, format->height - line.pos, range.color, range.decoration, c.face);
		break;
	}
}

void Label::writeQuads(VertexArray &vertexes, FormatSpec *format, Vector<ColorMask> &colorMap) {
	auto quadsCount = Label_getQuadsCount(format);
	colorMap.clear();
	colorMap.reserve(quadsCount);

	const font::RangeSpec *targetRange = nullptr;
	font::Metrics metrics;

	vertexes.clear();

	for (auto it = format->begin(); it != format->end(); ++ it) {
		if (it.count() == 0) {
			continue;
		}

		if (&(*it.range) != targetRange) {
			targetRange = &(*it.range);
			metrics = targetRange->layout->getMetrics();
		}

		const auto start = it.start();
		auto end = start + it.count();

		for (auto charIdx = start; charIdx < end; ++ charIdx) {
			const font::CharSpec &c = format->chars[charIdx];
			if (!string::isspace(c.charID) && c.charID != char16_t(0x0A) && c.charID != char16_t(0x00AD)) {

				uint16_t face = 0;
				auto ch = targetRange->layout->getChar(c.charID, face);

				if (ch.charID == c.charID) {
					auto quad = vertexes.addQuad();
					Label_pushColorMap(*it.range, colorMap);
					Label_writeTextureQuad(format, metrics, c, ch, *it.range, *it.line, quad);
				}
			}
		}

		if (it.line->start + it.line->count == end) {
			const font::CharSpec &c = format->chars[end - 1];
			if (c.charID == char16_t(0x00AD)) {
				uint16_t face = 0;
				auto ch = targetRange->layout->getChar(c.charID, face);

				if (ch.charID == c.charID) {
					auto quad = vertexes.addQuad();
					Label_pushColorMap(*it.range, colorMap);
					Label_writeTextureQuad(format, metrics, c, ch, *it.range, *it.line, quad);
				}
			}
			end -= 1;
		}

		if (it.count() > 0 && it.range->decoration != font::TextDecoration::None) {
			const font::CharSpec &firstChar = format->chars[it.start()];
			const font::CharSpec &lastChar = format->chars[it.start() + it.count() - 1];

			auto color = it.range->color;
			color.a = uint8_t(0.75f * color.a);
			auto metrics = it.range->layout->getMetrics();

			float offset = 0.0f;
			switch (it.range->decoration) {
			case font::TextDecoration::None: break;
			case font::TextDecoration::Overline:
				offset = metrics.height;
				break;
			case font::TextDecoration::LineThrough:
				offset = (metrics.height * 11.0f) / 24.0f;
				break;
			case font::TextDecoration::Underline:
				offset = metrics.height / 8.0f;
				break;
			}

			const float width = metrics.height / 16.0f;
			const float base = floorf(width);
			const float frac = width - base;

			const auto underlineBase = uint16_t(base);
			const auto underlineX = firstChar.pos;
			const auto underlineWidth = lastChar.pos + lastChar.advance - firstChar.pos;
			const auto underlineY = format->height - it.line->pos + offset - underlineBase / 2;
			const auto underlineHeight = underlineBase;

			auto quad = vertexes.addQuad();
			Label_pushColorMap(*it.range, colorMap);
			quad.drawUnderlineRect(underlineX, underlineY, underlineWidth, underlineHeight, color);
			if (frac > 0.1) {
				color.a *= frac;

				auto quad = vertexes.addQuad();
				Label_pushColorMap(*it.range, colorMap);
				quad.drawUnderlineRect(underlineX, underlineY - 1, underlineWidth, 1, color);
			}
		}
	}
}

Rc<LabelResult> Label::writeResult(FormatSpec *format, const Color4F &color) {
	auto result = Rc<LabelResult>::alloc();
	VertexArray array;
	array.init(format->chars.size() * 4, format->chars.size() * 6);

	writeQuads(array, format, result->colorMap);
	result->data.mat = Mat4::IDENTITY;
	result->data.data = array.pop();
	return result;
}

Label::~Label() {
	_format = nullptr;
}

bool Label::init() {
	return init(nullptr);
}

bool Label::init(StringView str) {
	return init(nullptr, DescriptionStyle(), str, 0.0f, Alignment::Left);
}

bool Label::init(StringView str, float w, Alignment a) {
	return init(nullptr, DescriptionStyle(), str, w, a);
}

bool Label::init(font::FontController *source, const DescriptionStyle &style,
		StringView str, float width, Alignment alignment) {
	if (!Sprite::init()) {
		return false;
	}

	if (!source) {
		source = Application::getInstance()->getFontController();
	}

	_source = source;
	_style = style;
	setNormalized(true);

	setColorMode(ColorMode::AlphaChannel);
	setRenderingLevel(RenderingLevel::Surface);

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

	setString(str);
	setWidth(width);
	setAlignment(alignment);

	return true;
}

bool Label::init(const DescriptionStyle &style, StringView str, float w, Alignment a) {
	return init(nullptr, style, str, w, a);
}

void Label::tryUpdateLabel() {
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
		_vertexesDirty = true;
		setContentSize(Size2(0.0f, getFontHeight() / _density));
		return;
	}

	auto spec = Rc<font::FormatSpec>::alloc(Rc<font::FontController>(_source), _string16.size(), _compiledStyles.size() + 1);

	_compiledStyles = compileStyle();
	_style.text.color = _displayedColor.getColor();
	_style.text.opacity = _displayedColor.getOpacity();
	_style.text.whiteSpace = font::WhiteSpace::PreWrap;

	if (!updateFormatSpec(spec, _compiledStyles, _density, _adjustValue)) {
		return;
	}

	_format = spec;

	if (_format) {
		if (_format->chars.empty()) {
			setContentSize(Size2(0.0f, getFontHeight() / _density));
		} else {
			setContentSize(Size2(_format->width / (_density), _format->height / (_density)));
		}

		_labelDirty = false;
		_vertexColorDirty = false;
		_vertexesDirty = true;
	}
}

void Label::onTransformDirty(const Mat4 &parent) {
	updateLabelScale(parent);
	Sprite::onTransformDirty(parent);
}

void Label::onGlobalTransformDirty(const Mat4 &parent) {
	if (!_transformDirty) {
		updateLabelScale(parent);
	}

	Sprite::onGlobalTransformDirty(parent);
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
	_vertexColorDirty = true;
}

void Label::updateVertexesColor() {
	if (_deferredResult) {
		_deferredResult->updateColor(_displayedColor);
	} else {
		if (!_colorMap.empty()) {
			_vertexes.updateColorQuads(_displayedColor, _colorMap);
		}
	}
}

void Label::updateQuadsForeground(font::FontController *controller, FormatSpec *format, Vector<ColorMask> &colorMap) {
	writeQuads(_vertexes, format, colorMap);
}

bool Label::checkVertexDirty() const {
	return _vertexesDirty || _labelDirty;
}

NodeFlags Label::processParentFlags(RenderFrameInfo &info, NodeFlags parentFlags) {
	if (_labelDirty) {
		updateLabel();
	}

	return Sprite::processParentFlags(info, parentFlags);
}

void Label::pushCommands(RenderFrameInfo &frame, NodeFlags flags) {
	if (_deferred) {
		if (!_deferredResult || (_deferredResult->isReady() && _deferredResult->getResult()->data.empty())) {
			return;
		}

		frame.commands->pushDeferredVertexResult(_deferredResult, frame.viewProjectionStack.back(), frame.modelTransformStack.back(),
				_normalized, frame.zPath, _materialId, _realRenderingLevel, _commandFlags);
	} else {
		Sprite::pushCommands(frame, flags);
	}
}

void Label::updateLabelScale(const Mat4 &parent) {
	Vec3 scale;
	parent.decompose(&scale, nullptr, nullptr);

	if (_scale.x != 1.f) { scale.x *= _scale.x; }
	if (_scale.y != 1.f) { scale.y *= _scale.y; }
	if (_scale.z != 1.f) { scale.z *= _scale.z; }

	auto density = std::min(std::min(scale.x, scale.y), scale.z);
	if (density != _density) {
		_density = density;
		_labelDirty = true;
	}

	if (_labelDirty) {
		updateLabel();
	}
}

void Label::setStandalone(bool value) {
	if (_standalone != value) {
		_standalone = value;
		//_standaloneMap.clear();
		//_standaloneChars.clear();
		_vertexesDirty = true;
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

void Label::updateVertexes() {
	if (!_source) {
		return;
	}

	if (_labelDirty) {
		updateLabel();
	}

	if (!_format || _format->chars.size() == 0 || _string16.empty()) {
		_vertexes.clear();
		return;
	}

	if (!_standalone) {
		for (auto &it : _format->ranges) {
			auto dep = _source->addTextureChars(it.layout->getId(), SpanView<font::CharSpec>(_format->chars, it.start, it.count));
			if (dep) {
				emplace_ordered(_pendingDependencies, move(dep));
			}
		}

		if (_deferred) {
			auto &manager = _director->getApplication()->getDeferredManager();
			_deferredResult = manager->runLabel(_format, _displayedColor);
			_vertexes.clear();
			_vertexColorDirty = false;
		} else {
			_deferredResult = nullptr;
			updateQuadsForeground(_source, _format, _colorMap);
			_vertexColorDirty = true;
		}
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
		_vertexesDirty = true;
	}
}

void Label::onFontSourceLoaded() {
	if (_source) {
		setTexture(Rc<Texture>(_source->getTexture()));
		_vertexesDirty = true;
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

void Label::setDeferred(bool val) {
	if (val != _deferred) {
		_deferred = val;
		_vertexesDirty = true;
	}
}

LabelDeferredResult::~LabelDeferredResult() {
	if (_future) {
		delete _future;
		_future = nullptr;
	}
}

bool LabelDeferredResult::init(std::future<Rc<LabelResult>> &&future) {
	_future = new std::future<Rc<LabelResult>>(move(future));
	return true;
}

SpanView<gl::TransformedVertexData> LabelDeferredResult::getData() {
	std::unique_lock<Mutex> lock(_mutex);
	if (_future) {
		_result = _future->get();
		delete _future;
		_future = nullptr;
		DeferredVertexResult::handleReady();
	}
	return makeSpanView(&_result->data, 1);
}

void LabelDeferredResult::handleReady(Rc<LabelResult> &&res) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_future) {
		delete _future;
		_future = nullptr;
	}
	_result = move(res);
	DeferredVertexResult::handleReady();
}

void LabelDeferredResult::updateColor(const Color4F &color) {
	getResult(); // ensure rendering was complete

	std::unique_lock<Mutex> lock(_mutex);
	if (_result) {
		VertexArray arr;
		arr.init(_result->data.data);
		arr.updateColorQuads(color, _result->colorMap);
		_result->data.data = arr.pop();
	}
}

Rc<gl::VertexData> LabelDeferredResult::getResult() const {
	std::unique_lock<Mutex> lock(_mutex);
	return _result->data.data;
}

}
