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

#include "MaterialFlexibleLayout.h"
#include "XLDirector.h"
#include "XLGlView.h"
#include "XLAction.h"
#include "MaterialEasing.h"

namespace stappler::xenolith::material {

void FlexibleLayout::NodeParams::setPosition(float x, float y) {
	setPosition(Vec2(x, y));
}

void FlexibleLayout::NodeParams::setPosition(const Vec2 &pos) {
	position = pos;
	mask = Mask(mask | Mask::Position);
}

void FlexibleLayout::NodeParams::setAnchorPoint(const Vec2 &pt) {
	anchorPoint = pt;
	mask = Mask(mask | Mask::AnchorPoint);
}

void FlexibleLayout::NodeParams::setContentSize(const Size2 &size) {
	contentSize = size;
	mask = Mask(mask | Mask::ContentSize);
}

void FlexibleLayout::NodeParams::setVisible(bool value) {
	visible = value;
	mask = Mask(mask | Mask::Visibility);
}

void FlexibleLayout::NodeParams::apply(Node *node) const {
	if (mask & Mask::AnchorPoint) {
		node->setAnchorPoint(anchorPoint);
	}
	if (mask & Mask::Position) {
		node->setPosition(position);
	}
	if (mask & Mask::ContentSize) {
		node->setContentSize(contentSize);
	}
	if (mask & Mask::Visibility) {
		node->setVisible(visible);
	}
}

bool FlexibleLayout::init() {
	if (!DecoratedLayout::init()) {
		return false;
	}

	setCascadeOpacityEnabled(true);

	return true;
}

void FlexibleLayout::onContentSizeDirty() {
	DecoratedLayout::onContentSizeDirty();
	if (_flexibleHeightFunction) {
		auto ret = _flexibleHeightFunction();
		_flexibleMinHeight = ret.first;
		_flexibleMaxHeight = ret.second;
	}

	_flexibleExtraSpace = 0.0f;
	updateFlexParams();
}

void FlexibleLayout::setBaseNode(ScrollView *node, ZOrder zOrder) {
	if (node != _baseNode) {
		if (_baseNode) {
			_baseNode->removeFromParent();
			_baseNode = nullptr;
		}
		_baseNode = node;
		if (_baseNode) {
			_baseNode->setScrollCallback(std::bind(&FlexibleLayout::onScroll, this,
					std::placeholders::_1, std::placeholders::_2));
			if (_baseNode->isVertical()) {
				_baseNode->setOverscrollFrontOffset(getCurrentFlexibleHeight());
			}
			if (!_baseNode->getParent()) {
				addChild(_baseNode, zOrder);
			}
		}
		_contentSizeDirty = true;
	}
}

void FlexibleLayout::setFlexibleNode(Node *node, ZOrder zOrder) {
	if (node != _flexibleNode) {
		if (_flexibleNode) {
			_flexibleNode->removeFromParent();
			_flexibleNode = nullptr;
		}
		_flexibleNode = node;
		if (_flexibleNode) {
			addChild(_flexibleNode, zOrder);
		}
		_contentSizeDirty = true;
	}
}

void FlexibleLayout::setFlexibleAutoComplete(bool value) {
	if (_flexibleAutoComplete != value) {
		_flexibleAutoComplete = value;
	}
}

void FlexibleLayout::setFlexibleMinHeight(float height) {
	if (_flexibleMinHeight != height) {
		_flexibleMinHeight = height;
		_contentSizeDirty = true;
	}
}

void FlexibleLayout::setFlexibleMaxHeight(float height) {
	if (_flexibleMaxHeight != height) {
		_flexibleMaxHeight = height;
		_contentSizeDirty = true;
	}
}

float FlexibleLayout::getFlexibleMinHeight() const {
	return _flexibleMinHeight;
}

float FlexibleLayout::getFlexibleMaxHeight() const {
	return _flexibleMaxHeight;
}

void FlexibleLayout::setFlexibleBaseNode(bool val) {
	if (_flexibleBaseNode != val) {
		_flexibleBaseNode = val;
		_contentSizeDirty = true;
	}
}

bool FlexibleLayout::isFlexibleBaseNode() const {
	return _flexibleBaseNode;
}

void FlexibleLayout::setFlexibleHeightFunction(const HeightFunction &cb) {
	_flexibleHeightFunction = cb;
	if (cb) {
		auto ret = cb();
		_flexibleMinHeight = ret.first;
		_flexibleMaxHeight = ret.second;
		_contentSizeDirty = true;
		_flexibleLevel = 1.0f;
	}
}

const FlexibleLayout::HeightFunction &FlexibleLayout::getFlexibleHeightFunction() const {
	return _flexibleHeightFunction;
}

void FlexibleLayout::updateFlexParams() {
	NodeParams decorParams, flexibleNodeParams, baseNodeParams;

	bool hasTopDecor = (_decorationMask & DecorationMask::Top) != DecorationMask::None;
	auto size = _contentSize;
	size.height -= _decorationPadding.bottom;
	float decor = _viewDecorationTracked ? _decorationPadding.top : 0.0f;
	float flexSize = _flexibleMinHeight + (_flexibleMaxHeight + decor - _flexibleMinHeight) * _flexibleLevel;

	if (flexSize >= _flexibleMaxHeight && _viewDecorationTracked) {
		float tmpDecor = (flexSize - _flexibleMaxHeight);
		decorParams.setContentSize(Size2(_contentSize.width - _decorationPadding.horizontal(), tmpDecor));
		size.height -= tmpDecor;
		flexSize = _flexibleMaxHeight;
		decorParams.setPosition(Vec2(_decorationPadding.left, _contentSize.height));
		decorParams.setVisible(true);
	} else if (_viewDecorationTracked) {
		decorParams.setVisible(false);
	} else {
		decorParams.setVisible(hasTopDecor);
		if (hasTopDecor) {
			size.height -= _decorationPadding.top;
		}
	}

	flexibleNodeParams.setPosition(_decorationPadding.left, size.height + _decorationPadding.bottom);
	flexibleNodeParams.setAnchorPoint(Vec2(0, 1));
	flexibleNodeParams.setContentSize(Size2(size.width - _decorationPadding.horizontal(), flexSize + _flexibleExtraSpace));
	flexibleNodeParams.setVisible(flexSize > 0.0f);

	if (_viewDecorationTracked && _director) {
		_director->getView()->setDecorationVisible(_flexibleLevel == 1.0f);
	}

	Padding padding;
	if (_baseNode) {
		padding = _baseNode->getPadding();
	}

	float baseNodeOffset = getCurrentFlexibleHeight();
	Padding baseNodePadding(padding.setTop(getCurrentFlexibleMax() + _baseNodePadding));

	baseNodeParams.setAnchorPoint(Vec2(0, 0));
	baseNodeParams.setPosition(_decorationPadding.left, _decorationPadding.bottom);

	if (_flexibleBaseNode) {
		baseNodeParams.setContentSize(Size2(size.width - _decorationPadding.horizontal(), size.height + decor));
	} else {
		baseNodeParams.setContentSize(Size2(size.width - _decorationPadding.horizontal(), size.height + decor - getCurrentFlexibleMax() - 0.0f));
		baseNodePadding = padding.setTop(4.0f);
		baseNodeOffset = 0.0f;
	}

	onDecorNode(decorParams);
	onFlexibleNode(flexibleNodeParams);
	onBaseNode(baseNodeParams, baseNodePadding, baseNodeOffset);
}

void FlexibleLayout::onScroll(float delta, bool finished) {
	auto size = _baseNode->getScrollableAreaSize();
	if (!isnan(size) && size < _contentSize.height) {
		clearFlexibleExpand(0.25f);
		setFlexibleLevel(1.0f);
		return;
	}

	clearFlexibleExpand(0.25f);
	if (!finished && delta != 0.0f) {
		const auto distanceFromStart = _baseNode->getDistanceFromStart();
		const auto trigger = _safeTrigger ? ( _flexibleMaxHeight - _flexibleMinHeight) : 8.0f;
		if (isnan(distanceFromStart) || distanceFromStart > trigger || delta < 0) {
			stopActionByTag(FlexibleLayout::AutoCompleteTag());
			float height = getCurrentFlexibleHeight();
			//float diff = delta.y / _baseNode->getGlobalScale().y;
			float newHeight = height - delta;
			if (delta < 0) {
				float max = getCurrentFlexibleMax();
				if (newHeight > max) {
					newHeight = max;
				}
			} else {
				if (newHeight < _flexibleMinHeight) {
					newHeight = _flexibleMinHeight;
				}
			}
			setFlexibleHeight(newHeight);
		}
	} else if (finished) {
		if (_flexibleAutoComplete) {
			if (_flexibleLevel < 1.0f && _flexibleLevel > 0.0f) {
				auto distanceFromStart = _baseNode->getDistanceFromStart();
				bool open =  (_flexibleLevel > 0.5) || (!isnan(distanceFromStart) && distanceFromStart < (_flexibleMaxHeight - _flexibleMinHeight));
				auto a = Rc<ActionProgress>::create(progress(0.0f, 0.3f, open?_flexibleLevel:(1.0f - _flexibleLevel)), open?1.0f:0.0f,
						[this] (float p) {
					setFlexibleLevel(p);
				});
				a->setSourceProgress(_flexibleLevel);
				a->setTag(FlexibleLayout::AutoCompleteTag());
				if (open) {
					runAction(makeEasing(move(a), EasingType::StandardAccelerate));
				} else {
					runAction(makeEasing(move(a), EasingType::StandardDecelerate));
				}
			}
		}
	}
}

float FlexibleLayout::getFlexibleLevel() const {
	return _flexibleLevel;
}

void FlexibleLayout::setFlexibleLevel(float value) {
	if (value > 1.0f) {
		value = 1.0f;
	} else if (value < 0.0f) {
		value = 0.0f;
	}

	if (value == _flexibleLevel) {
		return;
	}

	_flexibleLevel = value;
	updateFlexParams();
}

void FlexibleLayout::setFlexibleLevelAnimated(float value, float duration) {
	stopActionByTag("FlexibleLevel"_tag);
	if (duration <= 0.0f) {
		setFlexibleLevel(value);
	} else {
		auto a = Rc<Sequence>::create(makeEasing(Rc<ActionProgress>::create(
				duration, _flexibleLevel, value,
				[this] (float progress) {
			setFlexibleLevel(progress);
		}), EasingType::Emphasized), [this, value] {
			setFlexibleLevel(value);
		});
		a->setTag("FlexibleLevel"_tag);
		runAction(a);
	}
}

void FlexibleLayout::setFlexibleHeight(float height) {
	float size = getCurrentFlexibleMax() - _flexibleMinHeight;
	if (size > 0.0f) {
		float value = (height - _flexibleMinHeight) / (getCurrentFlexibleMax() - _flexibleMinHeight);
		setFlexibleLevel(value);
	} else {
		setFlexibleLevel(1.0f);
	}
}

void FlexibleLayout::setBaseNodePadding(float val) {
	if (_baseNodePadding != val) {
		_baseNodePadding = val;
		_contentSizeDirty = true;
	}
}
float FlexibleLayout::getBaseNodePadding() const {
	return _baseNodePadding;
}

float FlexibleLayout::getCurrentFlexibleHeight() const {
	return (getCurrentFlexibleMax() - _flexibleMinHeight) * _flexibleLevel + _flexibleMinHeight;
}

float FlexibleLayout::getCurrentFlexibleMax() const {
	return _flexibleMaxHeight + (_viewDecorationTracked?_decorationPadding.top:0);
}

void FlexibleLayout::onPush(SceneContent *l, bool replace) {
	DecoratedLayout::onPush(l, replace);

	if (_viewDecorationTracked && _director) {
		_director->getView()->setDecorationVisible(_flexibleLevel == 1.0f);
	}
}
void FlexibleLayout::onForegroundTransitionBegan(SceneContent *l, SceneLayout *overlay) {
	DecoratedLayout::onForegroundTransitionBegan(l, overlay);

	if (_viewDecorationTracked && _director) {
		_director->getView()->setDecorationVisible(_flexibleLevel == 1.0f);
	}
}

void FlexibleLayout::onDecorNode(const NodeParams &p) {
	p.apply(_decorationTop);
}

void FlexibleLayout::onFlexibleNode(const NodeParams &p) {
	if (_flexibleNode) {
		p.apply(_flexibleNode);
	}
}

void FlexibleLayout::onBaseNode(const NodeParams &p, const Padding &padding, float offset) {
	if (_baseNode) {
		p.apply(_baseNode);
		if (_baseNode->isVertical()) {
			_baseNode->setOverscrollFrontOffset(offset);
			_baseNode->setPadding(padding);
		}
	}
}

void FlexibleLayout::setSafeTrigger(bool value) {
	_safeTrigger = value;
}

bool FlexibleLayout::isSafeTrigger() const {
	return _safeTrigger;
}

void FlexibleLayout::expandFlexibleNode(float extraSpace, float duration) {
	stopActionByTag("FlexibleExtraSpace"_tag);
	stopActionByTag("FlexibleExtraClear"_tag);
	if (duration > 0.0f) {
		auto prevSpace = _flexibleExtraSpace;
		if (extraSpace > prevSpace) {
			auto a = makeEasing(Rc<ActionProgress>::create(
					duration, [this, extraSpace, prevSpace] (float p) {
				_flexibleExtraSpace = progress(prevSpace, extraSpace, p);
				updateFlexParams();
			}), EasingType::Emphasized);
			runAction(a, "FlexibleExtraSpace"_tag);
		} else {
			auto a = makeEasing(Rc<ActionProgress>::create(
					duration, [this, extraSpace, prevSpace] (float p) {
				_flexibleExtraSpace = progress(prevSpace, extraSpace, p);
				updateFlexParams();
			}), EasingType::Emphasized);
			runAction(a, "FlexibleExtraSpace"_tag);
		}
	} else {
		_flexibleExtraSpace = extraSpace;
		updateFlexParams();
	}
}

void FlexibleLayout::clearFlexibleExpand(float duration) {
	if (_flexibleExtraSpace == 0.0f) {
		return;
	}

	if (duration > 0.0f) {
		auto a = getActionByTag("FlexibleExtraClear"_tag);
		if (!a) {
			stopActionByTag("FlexibleExtraSpace"_tag);
			auto prevSpace = _flexibleExtraSpace;
			auto a = makeEasing(Rc<ActionProgress>::create(
					duration, [this, prevSpace] (float p) {
				_flexibleExtraSpace = progress(prevSpace, 0.0f, p);
				updateFlexParams();
			}), EasingType::Emphasized);
			runAction(a, "FlexibleExtraClear"_tag);
		}
	} else {
		_flexibleExtraSpace = 0.0f;
		updateFlexParams();
	}
}

}
