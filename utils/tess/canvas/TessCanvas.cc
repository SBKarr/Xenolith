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
#include "TessCursor.h"
#include "XLGlView.h"
#include "XLDirector.h"

namespace stappler::xenolith::tessapp {

TessCanvas::~TessCanvas() { }

bool TessCanvas::init() {
	if (!Node::init()) {
		return false;
	}

	auto inputListener = addInputListener(Rc<InputListener>::create());
	inputListener->addTouchRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		onTouch(ev);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseLeft}));

	inputListener->addTapRecognizer([this] (GestureEvent event, const GestureTap &tap) {
		std::cout << "Tap: " << tap.pos << ": " << tap.count << "\n";
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseLeft}));

	inputListener->addMoveRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		onMouseMove(ev);
		return true;
	});

	inputListener->setPointerEnterCallback([this] (bool pointerEnter) {
		return onPointerEnter(pointerEnter);
	});

	_cursor = addChild(Rc<TessCursor>::create());
	_cursor->setColor(Color::Black);
	_cursor->setContentSize(Size2(20, 20));
	_cursor->setPosition(Vec2(200.0f, 200.0f));
	_cursor->setVisible(false);

	_pathFill = addChild(Rc<VectorSprite>::create(Size2(0, 0)), 1);
	_pathFill->setColor(Color::Blue_100);
	_pathFill->setPosition(Vec2(0.0f, 0.0f));
	_pathFill->setVisible(false);

	_pathLines = addChild(Rc<VectorSprite>::create(Size2(0, 0)), 2);
	_pathLines->setColor(Color::Green_500);
	_pathLines->setPosition(Vec2(0.0f, 0.0f));
	_pathLines->setLineWidth(1.0f);
	_pathLines->setVisible(false);

	return true;
}

void TessCanvas::onEnter(Scene *scene) {
	Node::onEnter(scene);

	_pointerInWindow = _director->getView()->isPointerWithinWindow();
}

void TessCanvas::onContentSizeDirty() {
	Node::onContentSizeDirty();

	if (_test1) {
		_test1->setPosition(Vec2(_contentSize / 2.0f) - Vec2(0.0f, _contentSize.height / 4.0f));
	}
	if (_test2) {
		_test2->setPosition(Vec2(_contentSize / 2.0f) + Vec2(0.0f, _contentSize.height / 4.0f));
	}

	_pathFill->setContentSize(_contentSize);
	_pathFill->getImage()->setImageSize(_contentSize);

	_pathLines->setContentSize(_contentSize);
	_pathLines->getImage()->setImageSize(_contentSize);
}

void TessCanvas::onTouch(const InputEvent &ev) {
	auto loc = convertToNodeSpace(ev.currentLocation);

	switch (ev.data.event) {
	case InputEventName::Begin:
		if ((ev.data.modifiers & InputModifier::Ctrl) == InputModifier::None) {
			if (auto pt = getTouchedPoint(ev.currentLocation)) {
				_capturedPoint = pt;
			}
		}
		std::cout << "Begin: " << loc << "\n";
		break;
	case InputEventName::Move:
		if (_capturedPoint) {
			auto loc = convertToNodeSpace(ev.currentLocation);
			_capturedPoint->setPoint(loc);
			updatePoints();
		}
		std::cout << "Move: " << loc << "\n";
		break;
	case InputEventName::End:
		if (_capturedPoint) {

		} else if (ev.currentLocation.distance(ev.originalLocation) < TapDistanceAllowed
				&& (ev.currentTime - ev.originalTime) < TapIntervalAllowed.toMicros()) {
			onActionTouch(ev);
		} else {
			std::cout << "End: " << loc << "\n";
		}
		_capturedPoint = nullptr;
		break;
	case InputEventName::Cancel:
		std::cout << "Cancel: " << loc << "\n";
		_capturedPoint = nullptr;
		break;
	default:
		std::cout << "Unknown: " << loc << "\n";
		break;
	}
}

void TessCanvas::onMouseMove(const InputEvent &ev) {
	if (_cursor) {
		_currentLocation = convertToNodeSpace(ev.currentLocation);
		if (isTouchedNodeSpace(_currentLocation)) {
			_cursor->setPosition(_currentLocation);
			_cursor->setVisible(_pointerInWindow);
			if (_pointerInWindow) {
				Vec2 pos;
				bool touched = false;
				for (auto &it : _points) {
					if (it->isTouched(ev.currentLocation, 10)) {
						touched = true;
						pos = it->getPoint();
					}
				}
				if (touched) {
					_cursor->setState(TessCursor::Capture);
					_cursor->setPosition(pos);
				} else {
					_cursor->setState(TessCursor::Point);
				}
			}
		} else {
			_cursor->setVisible(false);
		}
	}
}

bool TessCanvas::onPointerEnter(bool value) {
	_pointerInWindow = value;
	if (_cursor) {
		_cursor->setVisible(_pointerInWindow && isTouchedNodeSpace(_currentLocation));
	}
	return true;
}

void TessCanvas::onActionTouch(const InputEvent &ev) {
	if ((ev.data.modifiers & InputModifier::Ctrl) != InputModifier::None) {
		auto it = _points.begin();
		while (it != _points.end()) {
			if ((*it)->isTouched(ev.currentLocation, 10)) {
				(*it)->removeFromParent();
				it = _points.erase(it);
				updatePoints();
				return;
			} else {
				++ it;
			}
		}
	} else {
		auto loc = convertToNodeSpace(ev.currentLocation);
		auto pt = Rc<TessPoint>::create(loc);

		_points.emplace_back(addChild(move(pt), 10));
		updatePoints();
	}
}

TessPoint * TessCanvas::getTouchedPoint(const Vec2 &pt) const {
	for (auto &it : _points) {
		if (it->isTouched(pt, 10)) {
			return it;
		}
	}
	return nullptr;
}

void TessCanvas::updatePoints() {
	if (_points.size() > 2) {
		_pathFill->setVisible(true);
		_pathLines->setVisible(true);

		_pathFill->getImage()->clear();
		_pathLines->getImage()->clear();

		auto pathFill = _pathFill->getImage()->addPath();
		auto pathLines = _pathLines->getImage()->addPath();

		for (auto &it : _points) {
			pathFill->lineTo(it->getPoint());
			pathLines->lineTo(it->getPoint());
		}
	} else {
		_pathFill->setVisible(false);
		_pathLines->setVisible(false);
	}
}

}
