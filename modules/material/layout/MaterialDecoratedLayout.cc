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

#include "MaterialDecoratedLayout.h"

namespace stappler::xenolith::material {

bool DecoratedLayout::init(ColorRole role) {
	if (!SceneLayout::init()) {
		return false;
	}

	_decorationMask = DecorationMask::All;

	_decorationTop = addChild(Rc<LayerSurface>::create(SurfaceStyle(role, NodeStyle::Filled)), ZOrderMax);
	_decorationTop->setAnchorPoint(Anchor::TopLeft);
	_decorationTop->setVisible(false);
	_decorationTop->setStyleDirtyCallback([this] (const SurfaceStyleData &style) {
		updateStatusBar(style);
	});

	_decorationBottom = addChild(Rc<LayerSurface>::create(SurfaceStyle(role, NodeStyle::Filled)), ZOrderMax);
	_decorationBottom->setAnchorPoint(Anchor::BottomLeft);
	_decorationBottom->setVisible(false);

	_decorationLeft = addChild(Rc<LayerSurface>::create(SurfaceStyle(role, NodeStyle::Filled)), ZOrderMax);
	_decorationLeft->setAnchorPoint(Anchor::BottomLeft);
	_decorationLeft->setVisible(false);

	_decorationRight = addChild(Rc<LayerSurface>::create(SurfaceStyle(role, NodeStyle::Filled)), ZOrderMax);
	_decorationRight->setAnchorPoint(Anchor::BottomRight);
	_decorationRight->setVisible(false);

	_background = addChild(Rc<LayerSurface>::create(SurfaceStyle(ColorRole::Background, NodeStyle::SurfaceTonal)), ZOrderMin);
	_background->setAnchorPoint(Anchor::Middle);

	return true;
}

void DecoratedLayout::onContentSizeDirty() {
	SceneLayout::onContentSizeDirty();

	if (_decorationPadding.left > 0.0f) {
		_decorationLeft->setPosition(Vec2::ZERO);
		_decorationLeft->setContentSize(Size2(_decorationPadding.left, _contentSize.height));
		_decorationLeft->setVisible(true);
	} else {
		_decorationLeft->setVisible(false);
	}

	if (_decorationPadding.right > 0.0f) {
		_decorationRight->setPosition(Vec2(_contentSize.width, 0.0f));
		_decorationRight->setContentSize(Size2(_decorationPadding.right, _contentSize.height));
		_decorationRight->setVisible(true);
	} else {
		_decorationRight->setVisible(false);
	}

	if (_decorationPadding.top > 0.0f) {
		_decorationTop->setPosition(Vec2(_decorationPadding.left, _contentSize.height));
		_decorationTop->setContentSize(Size2(_contentSize.width - _decorationPadding.horizontal(), _decorationPadding.top));
		_decorationTop->setVisible(true);
	} else {
		_decorationTop->setVisible(false);
	}

	if (_decorationPadding.bottom > 0.0f) {
		_decorationBottom->setPosition(Vec2(_decorationPadding.left, 0.0f));
		_decorationBottom->setContentSize(Size2(_contentSize.width - _decorationPadding.horizontal(), _decorationPadding.bottom));
		_decorationBottom->setVisible(true);
	} else {
		_decorationBottom->setVisible(false);
	}

	_background->setPosition(_contentSize / 2.0f);
	_background->setContentSize(_contentSize);

	updateStatusBar(_decorationTop->getStyleCurrent());
}

void DecoratedLayout::setDecorationColorRole(ColorRole role) {
	SurfaceStyle tmp;

	tmp = _decorationLeft->getStyleOrigin();
	tmp.colorRole = role;
	_decorationLeft->setStyle(tmp);

	tmp = _decorationRight->getStyleOrigin();
	tmp.colorRole = role;
	_decorationRight->setStyle(tmp);

	tmp = _decorationTop->getStyleOrigin();
	tmp.colorRole = role;
	_decorationTop->setStyle(tmp);

	tmp = _decorationBottom->getStyleOrigin();
	tmp.colorRole = role;
	_decorationBottom->setStyle(tmp);
}

ColorRole DecoratedLayout::getDecorationColorRole() const {
	return _decorationLeft->getStyleTarget().colorRole;
}

void DecoratedLayout::setViewDecorationTracked(bool value) {
	if (_viewDecorationTracked != value) {
		_viewDecorationTracked = value;
	}
}

bool DecoratedLayout::isViewDecorationTracked() const {
	return _viewDecorationTracked;
}

void DecoratedLayout::onForeground(SceneContent *l, SceneLayout *overlay) {
	updateStatusBar(_decorationTop->getStyleCurrent());
}

void DecoratedLayout::updateStatusBar(const SurfaceStyleData &style) {
	if (_director && _decorationStyleTracked) {
		_director->getView()->setDecorationTone(style.colorOn.data.tone / 50.0f);
	}
}

}
