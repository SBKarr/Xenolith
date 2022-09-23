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
#include "SPFilesystem.h"

namespace stappler::xenolith::tessapp {

Color TessCanvas::getColorForIndex(uint32_t idx) {
	switch (idx % 4) {
	case 0: return Color::Red_500; break;
	case 1: return Color::Green_500; break;
	case 2: return Color::Blue_500; break;
	case 3: return Color::Purple_500; break;
	}
	return Color::Red_500;
}

TessCanvas::~TessCanvas() { }

bool TessCanvas::init(Function<void()> &&cb) {
	if (!Node::init()) {
		return false;
	}

	_onContourUpdated = move(cb);

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

	InputListener::KeyMask keys;
	keys.set(toInt(InputKeyCode::W));
	keys.set(toInt(InputKeyCode::A));
	keys.set(toInt(InputKeyCode::S));
	keys.set(toInt(InputKeyCode::D));

	inputListener->addKeyRecognizer([this] (GestureEvent event, const InputEvent &ev) {
		std::cout << event << " " << ev.data.key.keycode << " (" << ev.data.key.keysym << ")\n";

		return true;
	}, move(keys));

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
	_pathFill->setOpacity(0.5f);
	_pathFill->setRenderingLevel(RenderingLevel::Transparent);

	_pathLines = addChild(Rc<VectorSprite>::create(Size2(0, 0)), 2);
	_pathLines->setColor(Color::Green_500);
	_pathLines->setPosition(Vec2(0.0f, 0.0f));
	_pathLines->setLineWidth(1.0f);
	_pathLines->setVisible(false);
	//_pathLines->setRenderingLevel(RenderingLevel::Surface);

	auto path = filesystem::writablePath<Interface>("path.cbor");
	filesystem::mkdir(filepath::root(path));
	if (filesystem::exists(path)) {
		auto loadArray = [&] (ContourData &c, const Value &val) {
			for (auto &it : val.asArray()) {
				Vec2 point(it.getDouble(0), it.getDouble(1));
				auto pt = Rc<TessPoint>::create(point, c.points.size());

				c.points.emplace_back(addChild(move(pt), 10));
			}
		};

		auto val = data::readFile<Interface>(path);
		if (val.isArray()) {
			auto &c = _contours.emplace_back(ContourData{0});
			loadArray(c, val);
		} else if (val.isDictionary()) {
			auto ncontours = val.getInteger("ncontours");
			_contours.reserve(ncontours);

			for (auto &it : val.getArray("contours")) {
				auto &c = _contours.emplace_back(ContourData{uint32_t(_contours.size())});
				loadArray(c, it);
			}
		}
	}

	updatePoints();

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

void TessCanvas::setWinding(vg::Winding w) {
	if (w != _winding) {
		_winding = w;
		updatePoints();
	}
}

void TessCanvas::setDrawStyle(vg::DrawStyle s) {
	if (_drawStyle != s) {
		_drawStyle = s;
		updatePoints();
	}
}

void TessCanvas::setSelectedContour(uint32_t n) {
	_contourSelected = n % _contours.size();
	_onContourUpdated();
}

uint32_t TessCanvas::getSelectedContour() const {
	return _contourSelected;
}

uint32_t TessCanvas::getContoursCount() const {
	return _contours.size();
}

void TessCanvas::addContour() {
	if (_contours.back().points.size() > 0) {
		_contours.emplace_back(ContourData{uint32_t(_contours.size())});
		_contourSelected = _contours.size() - 1;
		_onContourUpdated();
	}
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
			loc = Vec2(roundf(loc.x), roundf(loc.y));
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
		_currentLocation = Vec2(roundf(_currentLocation.x), roundf(_currentLocation.y));
		if (isTouchedNodeSpace(_currentLocation)) {
			_cursor->setPosition(_currentLocation);
			_cursor->setVisible(_pointerInWindow);
			if (_pointerInWindow) {
				Vec2 pos;
				bool touched = false;
				for (auto &c : _contours) {
					for (auto &it : c.points) {
						if (it->isTouched(ev.currentLocation, 10)) {
							touched = true;
							pos = it->getPoint();
						}
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
		auto cIt = _contours.begin();
		while (cIt != _contours.end()) {
			auto it = cIt->points.begin();
			while (it != cIt->points.end()) {
				if ((*it)->isTouched(ev.currentLocation, 10)) {
					(*it)->removeFromParent();
					it = cIt->points.erase(it);

					while (it != cIt->points.end()) {
						(*it)->setIndex((*it)->getIndex() - 1);
						++ it;
					}

					if (cIt->points.size() == 0 && _contours.size() != 0) {
						_contours.erase(cIt);
						_onContourUpdated();
					}
					updatePoints();
					return;
				} else {
					++ it;
				}
			}
			++ cIt;
		}
	} else {
		if (_contourSelected == 0 && _contours.empty()) {
			_contours.emplace_back(ContourData{0});
		}

		auto loc = convertToNodeSpace(ev.currentLocation);
		loc = Vec2(roundf(loc.x), roundf(loc.y));

		auto &c = _contours[_contourSelected];
		auto pt = Rc<TessPoint>::create(loc, c.points.size());
		pt->setColor(getColorForIndex(_contourSelected));
		c.points.emplace_back(addChild(move(pt), 10));
		updatePoints();
	}
}

TessPoint * TessCanvas::getTouchedPoint(const Vec2 &pt) const {
	for (auto &c : _contours) {
		for (auto &it : c.points) {
			if (it->isTouched(pt, 10)) {
				return it;
			}
		}
	}
	return nullptr;
}

void TessCanvas::updatePoints() {
	uint32_t nContours = 0;

	_pathFill->getImage()->clear();
	_pathLines->getImage()->clear();

	auto pathFill = _pathFill->getImage()->addPath();
	auto pathLines = _pathLines->getImage()->addPath();

	pathFill->setWindingRule(_winding);
	pathFill->setStyle(_drawStyle);
	pathFill->setStrokeWidth(25.0f);
	pathFill->setStrokeColor(Color::Red_200);
	pathFill->setAntialiased(false);

	pathLines->setWindingRule(_winding);
	pathLines->setStyle(_drawStyle);
	pathLines->setStrokeWidth(25.0f);
	pathLines->setStrokeColor(Color::Red_200);
	pathLines->setAntialiased(false);

	for (const ContourData &contour : _contours) {
		if (contour.points.size() > 2) {
			for (auto &it : contour.points) {
				pathFill->lineTo(it->getPoint());
				pathLines->lineTo(it->getPoint());
				it->setColor(getColorForIndex(contour.index));
			}

			pathFill->closePath();
			pathLines->closePath();

			++ nContours;
		}
	}

	if (nContours) {
		_pathFill->setVisible(true);
		_pathLines->setVisible(true);
	} else {
		_pathFill->setVisible(false);
		_pathLines->setVisible(false);
	}

	auto path = filesystem::writablePath<Interface>("path.cbor");
	filesystem::remove(path);

	Value val;
	val.setInteger(_contours.size(), "ncountours");
	auto &c = val.emplace("contours");

	for (const ContourData &contour : _contours) {
		Value vals;
		for (auto &it : contour.points) {
			vals.addValue(Value({ Value(it->getPoint().x), Value(it->getPoint().y)}));
		}
		c.addValue(move(vals));
	}

	data::save(val, path,data::EncodeFormat::Cbor);
}

}
