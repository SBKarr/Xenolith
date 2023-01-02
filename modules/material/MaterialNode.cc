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

#include "MaterialNode.h"
#include "MaterialStyleContainer.h"
#include "XLAction.h"

namespace stappler::xenolith::material {

StyleData StyleData::progress(const StyleData &l, const StyleData &r, float p) {
	StyleData ret(r);
	ret.colorHCT = stappler::progress(l.colorHCT, r.colorHCT, p);
	ret.colorBackground = stappler::progress(l.colorBackground, r.colorBackground, p);
	ret.colorScheme = ret.colorHCT;
	ret.elevationValue = stappler::progress(l.elevationValue, r.elevationValue, p);

	ret.colorElevation = ret.colorBackground.asColor4F() * (1.0f - ret.elevationValue) + ret.colorScheme * ret.elevationValue;

	if (l.shapeFamily == r.shapeFamily) {
		ret.cornerRadius = stappler::progress(l.cornerRadius, r.cornerRadius, p);
	} else {
		auto scale = l.cornerRadius / (l.cornerRadius + r.cornerRadius);
		if (p < scale) {
			ret.shapeStyle = l.shapeStyle;
			ret.cornerRadius = stappler::progress(l.cornerRadius, 0.0f, p / scale);
		} else {
			ret.shapeStyle = r.shapeStyle;
			ret.cornerRadius = stappler::progress(0.0f, r.cornerRadius, p / (1.0f - scale));
		}
	}

	ret.shadowValue = stappler::progress(l.shadowValue, r.shadowValue, p);

	return ret;
}

void StyleData::apply(const Size2 &contentSize, const StyleContainer *style) {
	if (schemeName.empty()) {
		colorHCT = style->getPrimaryScheme().hct(colorRule);
		colorBackground = style->getPrimaryScheme().hct(ColorRole::Background);
	} else {
		if (auto scheme = style->getExtraScheme(schemeName)) {
			colorHCT = scheme->hct(colorRule);
			colorBackground = scheme->hct(ColorRole::Background);
		} else {
			colorHCT = style->getPrimaryScheme().hct(colorRule);
			colorBackground = style->getPrimaryScheme().hct(ColorRole::Background);
		}
	}

	colorScheme = colorHCT.asColor4F();

	switch (elevation) {
	case Elevation::Level0:
		elevationValue = 0.0f;
		shadowValue = hasShadow ? 0.0f : 0.0f;
		break;	// 0dp
	case Elevation::Level1:
		elevationValue = 0.05f;
		shadowValue = hasShadow ? 1.0f : 0.0f;
		break; // 1dp 5%
	case Elevation::Level2:
		elevationValue = 0.08f;
		shadowValue = hasShadow ? 3.0f : 0.0f;
		break; // 3dp 8%
	case Elevation::Level3:
		elevationValue = 0.11f;
		shadowValue = hasShadow ? 6.0f : 0.0f;
		break; // 6dp 11%
	case Elevation::Level4:
		elevationValue = 0.12f;
		shadowValue = hasShadow ? 8.0f : 0.0f;
		break; // 8dp 12%
	case Elevation::Level5:
		elevationValue = 0.14f;
		shadowValue = hasShadow ? 12.0f : 0.0f;
		break; // 12dp 14%
	}

	colorElevation = colorBackground.asColor4F() * (1.0f - elevationValue) + colorScheme * elevationValue;

	switch (shapeStyle) {
	case ShapeStyle::None: cornerRadius = 0.0f; break; // 0dp
	case ShapeStyle::ExtraSmall: cornerRadius = 4.0f; break; // 4dp
	case ShapeStyle::Small: cornerRadius = 8.0f; break; // 8dp
	case ShapeStyle::Medium: cornerRadius = 12.0f; break; // 12dp
	case ShapeStyle::Large: cornerRadius = 16.0f; break; // 16dp
	case ShapeStyle::ExtraLarge: cornerRadius = 28.0f; break; // 28dp
	case ShapeStyle::Full: cornerRadius = std::min(contentSize.width, contentSize.height) / 2.0f; break; // .
	}
}

bool MaterialNode::init(StyleData &&style) {
	if (!VectorSprite::init(Size2(8.0f, 8.0f))) {
		return false;
	}

	_styleOrigin = _styleTarget = move(style);
	_styleDirty = true;
	return true;
}

void MaterialNode::setStyle(StyleData &&style) {
	if (_inTransition) {
		_styleDirty = true;
		stopAllActionsByTag(TransitionActionTag);
		_inTransition = false;
		_styleProgress = 0.0f;
	}

	if (_styleOrigin != style) {
		_styleOrigin = _styleTarget = move(style);
		_styleDirty = true;
	}
}

void MaterialNode::setStyle(StyleData &&style, float duration) {
	if (_inTransition) {
		_styleDirty = true;
		stopAllActionsByTag(TransitionActionTag);
		_inTransition = false;
		_styleProgress = 0.0f;
	}

	if (_styleOrigin != style) {
		_styleTarget = move(style);
		runAction(Rc<ActionProgress>::create(duration, [this] (float progress) {
			_styleProgress = progress;
			_styleDirty = true;
		}, [this] {
			_inTransition = true;
		}, [this] {
			_styleOrigin = _styleTarget;
			_styleDirty = true;
			_inTransition = false;
			_styleProgress = 0.0f;
		}), TransitionActionTag);
		_styleDirty = true;
	}
}

bool MaterialNode::visitDraw(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	if (_styleDirty || _contentSizeDirty) {
		auto ud = frame.userdata.cast<StyleContainer>();
		if (ud) {
			_styleTarget.apply(_contentSize, ud);
			_styleOrigin.apply(_contentSize, ud);
		}

		if (_styleProgress > 0.0f) {
			_styleCurrent = StyleData::progress(_styleOrigin, _styleTarget, _styleProgress);
		} else {
			_styleCurrent = _styleOrigin;
		}
		applyStyle(_styleCurrent);
	}

	return VectorSprite::visitDraw(frame, parentFlags);
}

void MaterialNode::applyStyle(const StyleData &style) {
	auto radius = std::min(std::min(_contentSize.width / 2.0f, _contentSize.height / 2.0f), style.cornerRadius);

	if (radius != _realCornerRadius || _contentSize != _image->getImageSize()) {
		auto img = Rc<VectorImage>::create(_contentSize);
		auto path = img->addPath();
		if (radius > 0.0f) {
			switch (style.shapeFamily) {
			case ShapeFamily::RoundedCorners:
				path->moveTo(0.0f, radius)
					.arcTo(radius, radius, 0.0f, false, true, radius, 0.0f)
					.lineTo(_contentSize.width - radius, 0.0f)
					.arcTo(radius, radius, 0.0f, false, true, _contentSize.width, radius)
					.lineTo(_contentSize.width, _contentSize.height - radius)
					.arcTo(radius, radius, 0.0f, false, true, _contentSize.width - radius, _contentSize.height)
					.lineTo(radius, _contentSize.height)
					.arcTo(radius, radius, 0.0f, false, true, 0.0f, _contentSize.height - radius)
					.closePath()
					.setAntialiased(false)
					.setFillColor(Color::White)
					.setStyle(vg::DrawStyle::Fill);
				break;
			case ShapeFamily::CutCorners:
				path->moveTo(0.0f, radius)
					.lineTo(radius, 0.0f)
					.lineTo(_contentSize.width - radius, 0.0f)
					.lineTo(_contentSize.width, radius)
					.lineTo(_contentSize.width, _contentSize.height - radius)
					.lineTo(_contentSize.width - radius, _contentSize.height)
					.lineTo(radius, _contentSize.height)
					.lineTo(0.0f, _contentSize.height - radius)
					.closePath()
					.setAntialiased(false)
					.setFillColor(Color::White)
					.setStyle(vg::DrawStyle::Fill);
				break;
			}
		} else {
			path->moveTo(0.0f, 0.0f)
				.lineTo(_contentSize.width, 0.0f)
				.lineTo(_contentSize.width, _contentSize.height)
				.lineTo(0.0f, _contentSize.height)
				.closePath()
				.setAntialiased(false)
				.setFillColor(Color::White)
				.setStyle(vg::DrawStyle::Fill);
		}
		_realCornerRadius = radius;

		setImage(move(img));
	}

	setColor(style.colorElevation);
	setShadowIndex(style.shadowValue);
}

}
