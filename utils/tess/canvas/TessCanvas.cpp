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
#include "XLInputListener.h"
#include "TessCanvas.h"
#include "XLGlView.h"
#include "XLDirector.h"

namespace stappler::xenolith::tessapp {

bool TessCanvas::init() {
	if (!Node::init()) {
		return false;
	}

	auto inputListener = addInputListener(Rc<InputListener>::create());
	inputListener->addTouchRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		onTouch(ev);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseLeft}));

	inputListener->addMoveRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		onMouseMove(ev);
		return true;
	});

	inputListener->setPointerEnterCallback([this] (bool pointerEnter) {
		return onPointerEnter(pointerEnter);
	});

	auto image = Rc<VectorImage>::create(Size2(10, 10));
	auto path = image->addPath();
	path->setFillColor(Color::White);
	//path->addRect(Rect(0, 0, 10, 10));
	path->addOval(Rect(0, 0, 10, 10));
	path->setAntialiased(false);

	_cursor = addChild(Rc<VectorSprite>::create(move(image)));
	_cursor->setColor(Color::Black);
	_cursor->setContentSize(Size2(10, 10));
	_cursor->setAutofit(Sprite::Autofit::Contain);
	_cursor->setPosition(Vec2(200.0f, 200.0f));
	_cursor->setAnchorPoint(Anchor::Middle);
	_cursor->setVisible(false);
	//_cursor->setRenderingLevel(RenderingLevel::Solid);

	return true;
}

void TessCanvas::onEnter(Scene *scene) {
	Node::onEnter(scene);

	_pointerInWindow = _director->getView()->isPointerWithinWindow();
}

void TessCanvas::onTouch(const InputEvent &ev) {

}

void TessCanvas::onMouseMove(const InputEvent &ev) {
	_currentLocation = convertToNodeSpace(ev.currentLocation);
	if (isTouchedNodeSpace(_currentLocation)) {
		_cursor->setPosition(_currentLocation);
		_cursor->setVisible(_pointerInWindow);
	} else {
		_cursor->setVisible(false);
	}
}

bool TessCanvas::onPointerEnter(bool value) {
	_pointerInWindow = value;
	_cursor->setVisible(_pointerInWindow && isTouchedNodeSpace(_currentLocation));
	return true;
}

}
