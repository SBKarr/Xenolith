/**
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

#include "MaterialInputTextContainer.h"
#include "MaterialSurfaceInterior.h"
#include "MaterialStyleContainer.h"
#include "XLRenderFrameInfo.h"
#include "XLLayer.h"

namespace stappler::xenolith::material {

InputTextContainer::~InputTextContainer() { }

bool InputTextContainer::init() {
	if (!DynamicStateNode::init()) {
		return false;
	}

	_label = addChild(Rc<TypescaleLabel>::create(TypescaleRole::BodyLarge), 1);
	_label->setAnchorPoint(Anchor::BottomLeft);

	_caret = _label->addChild(Rc<Layer>::create());
	_caret->setAnchorPoint(Anchor::BottomLeft);
	_caret->setOpacity(0.0f);

	enableScissor(Padding(0.0f, 2.0f));

	return true;
}

void InputTextContainer::onContentSizeDirty() {
	DynamicStateNode::onContentSizeDirty();

	_label->setPosition(Vec2(0.0f, 0.0f) + Vec2(_label->getContentSize() - _contentSize) * _adjustment);
	_caret->setContentSize(Size2(1.0f, _label->getFontHeight()));
}

bool InputTextContainer::visitDraw(RenderFrameInfo &frame, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	if (_cursorDirty) {
		updateCursorPosition();
		_cursorDirty = false;
	}

	auto style = frame.getComponent<SurfaceInterior>(SurfaceInterior::ComponentFrameTag);
	auto styleContainer = frame.getComponent<StyleContainer>(StyleContainer::ComponentFrameTag);
	if (style && styleContainer) {
		if (auto scheme = styleContainer->getScheme(style->getStyle().schemeTag)) {
			auto c = scheme->get(ColorRole::Primary);
			auto currentColor = _caret->getColor();
			currentColor.a = 1.0f;
			if (currentColor != c) {
				_caret->setColor(c, false);
			}
		}
	}

	return DynamicStateNode::visitDraw(frame, parentFlags);
}

void InputTextContainer::setEnabled(bool value) {
	if (value != _enabled) {
		_enabled = value;
		_caret->stopAllActions();
		_caret->runAction(makeEasing(Rc<FadeTo>::create(0.2f, _enabled ? 1.0f : 0.0f)));
	}
}

void InputTextContainer::setCursor(TextCursor cursor) {
	if (_cursor != cursor) {
		_cursor = cursor;
		_cursorDirty = true;
		updateCursorPosition();
	}
}

void InputTextContainer::handleLabelChanged() {
	_cursorDirty = true;
}

void InputTextContainer::handleLabelPositionChanged() {

}

void InputTextContainer::updateCursorPosition() {
	Vec2 cpos;
	if (_label->empty()) {
		cpos = Vec2(0.0f, 0.0f);
	} else {
		cpos = _label->getCursorPosition(_cursor.start);
	}
	_caret->setPosition(cpos);

	auto labelWidth = _label->getContentSize().width;
	auto width = _contentSize.width;
	auto minPos = width - std::max(labelWidth, cpos.x);
	auto maxPos = 0.0f;

	if (labelWidth <= width) {
		runAdjustLabel(0.0f);
	} else {
		if (cpos.x < 0.0f || cpos.x >= _contentSize.width) {
			auto labelPos = cpos.x + width / 2;
			if (labelWidth > width && labelPos > width) {
				auto newpos = width - labelPos;
				if (newpos < minPos) {
					newpos = minPos;
				} else if (newpos > maxPos) {
					newpos = maxPos;
				}

				runAdjustLabel(newpos);
			}
		}
	}
}

void InputTextContainer::runAdjustLabel(float pos) {
	if (_label->getPosition().x == pos) {
		_label->stopAllActionsByTag("InputTextContainerAdjust"_tag);
		return;
	}

	_label->stopAllActionsByTag("InputTextContainerAdjust"_tag);

	const float minT = 0.05f;
	const float maxT = 0.25f;
	const float labelPos = _label->getPosition().x;

	auto dist = std::fabs(labelPos - pos);

	if (_enabled) {
		if (dist > _contentSize.width * 0.25f) {
			auto targetPos = labelPos - std::copysign(dist - _contentSize.width * 0.25f, labelPos - pos);
			_label->setPositionX(targetPos);
			dist = _contentSize.width * 0.25f;
		}
	}

	auto t = minT;
	if (dist < 20.0f) {
		t = minT;
	} else if (dist > 220.0f) {
		t = maxT;
	} else {
		t = progress(minT, maxT, (dist - 20.0f) / 200.0f);
	}

	auto a = makeEasing(Rc<MoveTo>::create(t, Vec2(pos, _label->getPosition().y)));
	_label->runAction(a, "InputTextContainerAdjust"_tag);
}

}
