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

#include "MaterialNode.h"
#include "XLAction.h"
#include "XLRenderFrameInfo.h"

namespace stappler::xenolith::material {

uint64_t MaterialNodeInterior::ComponentFrameTag = Component::GetNextComponentId();

bool MaterialNodeInterior::init() {
	if (!Component::init()) {
		return false;
	}

	_frameTag = ComponentFrameTag;
	return true;
}

bool MaterialNodeInterior::init(StyleData &&style) {
	if (!Component::init()) {
		return false;
	}

	_frameTag = ComponentFrameTag;
	_interiorStyle = move(style);
	return true;
}

void MaterialNodeInterior::onAdded(Node *owner) {
	Component::onAdded(owner);

	_ownerIsMaterialNode = (dynamic_cast<MaterialNode *>(_owner) != nullptr);
}

void MaterialNodeInterior::visit(RenderFrameInfo &info, NodeFlags parentFlags) {
	Component::visit(info, parentFlags);

	if (!_ownerIsMaterialNode) {
		auto style = info.getComponent<StyleContainer>(StyleContainer::ComponentFrameTag);
		if (!style) {
			return;
		}

		_interiorStyle.apply(_owner->getContentSize(), style);
	}
}

bool MaterialNode::init(StyleData &&style) {
	if (!VectorSprite::init(Size2(8.0f, 8.0f))) {
		return false;
	}

	_interior = addComponent(Rc<MaterialNodeInterior>::create());

	_styleOrigin = _styleTarget = move(style);
	_styleDirty = true;
	return true;
}

bool MaterialNode::init(const StyleData &style) {
	if (!VectorSprite::init(Size2(8.0f, 8.0f))) {
		return false;
	}

	_interior = addComponent(Rc<MaterialNodeInterior>::create());

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
		runAction(makeEasing(Rc<ActionProgress>::create(duration, [this] (float progress) {
			_styleProgress = progress;
			_styleDirty = true;
		}, [this] {
			_inTransition = true;
		}, [this] {
			_styleOrigin = _styleTarget;
			_styleDirty = true;
			_inTransition = false;
			_styleProgress = 0.0f;
		})), TransitionActionTag);
		_styleDirty = true;
	}
}

bool MaterialNode::visitDraw(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	auto style = frame.getComponent<StyleContainer>(StyleContainer::ComponentFrameTag);
	if (!style) {
		return false;
	}

	if (style) {
		if (_styleTarget.apply(_contentSize, style)) {
			_styleDirty = true;
		}
		if (_styleOrigin.apply(_contentSize, style)) {
			_styleDirty = true;
		}
	}

	if (_styleDirty || _contentSizeDirty) {
		if (_styleProgress > 0.0f) {
			_styleCurrent = StyleData::progress(_styleOrigin, _styleTarget, _styleProgress);
		} else {
			_styleCurrent = _styleOrigin;
		}
		applyStyle(_styleCurrent);
		_interior->setStyle(StyleData(_styleCurrent));
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
