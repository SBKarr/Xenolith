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

#include "AppVgTessCanvas.h"
#include "XLInputListener.h"
#include "XLDirector.h"

namespace stappler::xenolith::app {

bool VgTessCursor::init() {
	auto image = Rc<VectorImage>::create(Size2(64, 64));
	updateState(*image, _state);

	if (!VectorSprite::init(move(image))) {
		return false;
	}

	setAutofit(Sprite::Autofit::Contain);
	setAnchorPoint(Anchor::Middle);

	return true;
}

void VgTessCursor::setState(State state) {
	if (_state != state) {
		_state = state;
		updateState(*_image, _state);
	}
}

void VgTessCursor::updateState(VectorImage &image, State state) {
	switch (state) {
	case Point:
		image.clear();
		image.addPath("", "org.stappler.xenolith.tess.TessCursor.Point")
			->setFillColor(Color::White)
			.addOval(Rect(16, 16, 32, 32))
			.setAntialiased(false);
		break;
	case Capture:
		image.clear();
		image.addPath("", "org.stappler.xenolith.tess.TessCursor.Capture")
			->setFillColor(Color::White)
			.moveTo(0, 24) .lineTo(4, 24) .lineTo(4, 4) .lineTo(24, 4) .lineTo(24, 0) .lineTo(0, 0)
			.moveTo(0, 40) .lineTo(0, 64) .lineTo(24, 64) .lineTo(24, 60) .lineTo(4, 60) .lineTo(4, 40)
			.moveTo(40, 64) .lineTo(64, 64) .lineTo(64, 40) .lineTo(60, 40) .lineTo(60, 60) .lineTo(40, 60)
			.moveTo(40, 0) .lineTo(64, 0) .lineTo(64, 24) .lineTo(60, 24) .lineTo(60, 4) .lineTo(40, 4)
			.setAntialiased(false);
		break;
	case Target:
		image.clear();
		image.addPath("", "org.stappler.xenolith.tess.TessCursor.Target")
			->setFillColor(Color::White)
			.moveTo(0.0f, 30.0f)
			.lineTo(0.0f, 34.0f)
			.lineTo(30.0f, 34.0f)
			.lineTo(30.0f, 64.0f)
			.lineTo(34.0f, 64.0f)
			.lineTo(34.0f, 34.0f)
			.lineTo(64.0f, 34.0f)
			.lineTo(64.0f, 30.0f)
			.lineTo(34.0f, 30.0f)
			.lineTo(34.0f, 0.0f)
			.lineTo(30.0f, 0.0f)
			.lineTo(30.0f, 30.0f)
			.setAntialiased(false);
		break;
	}
}

bool VgTessPoint::init(const Vec2 &p, uint32_t index) {
	auto image = Rc<VectorImage>::create(Size2(10, 10));
	image->addPath("", "org.stappler.xenolith.tess.TessPoint")
		->setFillColor(Color::White)
		.addOval(Rect(0, 0, 10, 10))
		.setAntialiased(false);

	if (!VectorSprite::init(move(image))) {
		return false;
	}

	_label = addChild(Rc<Label>::create());
	_label->setFontSize(18);
	_label->setFontWeight(Label::FontWeight::Bold);
	_label->setColor(Color::Black, true);
	_label->setString(toString(index, "; ", p.x, " ", p.y));
	_label->setPosition(Vec2(12, 12));

	setAnchorPoint(Anchor::Middle);
	setPosition(p);
	setColor(Color::Red_500);
	_point = p;
	_index = index;
	return true;
}

void VgTessPoint::setPoint(const Vec2 &pt) {
	_point = pt;
	setPosition(pt);
	_label->setString(toString(_index, "; ", _point.x, " ", _point.y));
}

void VgTessPoint::setIndex(uint32_t index) {
	if (_index != index) {
		_label->setString(toString(index, "; ", _point.x, " ", _point.y));
		_index = index;
	}
}

Color VgTessCanvas::getColorForIndex(uint32_t idx) {
	switch (idx % 4) {
	case 0: return Color::Red_500; break;
	case 1: return Color::Green_500; break;
	case 2: return Color::Blue_500; break;
	case 3: return Color::Purple_500; break;
	}
	return Color::Red_500;
}

VgTessCanvas::~VgTessCanvas() { }

bool VgTessCanvas::init(Function<void()> &&cb) {
	if (!Node::init()) {
		return false;
	}

	_onContourUpdated = move(cb);

	auto inputListener = addInputListener(Rc<InputListener>::create());
	inputListener->addTouchRecognizer([this] (const GestureData &ev) {
		onTouch(*ev.input);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::MouseLeft}));

	inputListener->addMoveRecognizer([this] (const GestureData &ev) {
		onMouseMove(*ev.input);
		return true;
	});

	InputListener::KeyMask keys;
	keys.set(toInt(InputKeyCode::W));
	keys.set(toInt(InputKeyCode::A));
	keys.set(toInt(InputKeyCode::S));
	keys.set(toInt(InputKeyCode::D));

	inputListener->addKeyRecognizer([] (const GestureData &ev) {
		std::cout << ev.event << " " << ev.input->data.key.keycode << " (" << ev.input->data.key.keysym << ")\n";

		return true;
	}, move(keys));

	inputListener->setPointerEnterCallback([this] (bool pointerEnter) {
		return onPointerEnter(pointerEnter);
	});

	_cursor = addChild(Rc<VgTessCursor>::create());
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
				auto pt = Rc<VgTessPoint>::create(point, c.points.size());

				c.points.emplace_back(addChild(move(pt), 10));
			}
		};

		auto val = data::readFile<Interface>(path);
		if (val.isArray()) {
			auto &c = _contours.emplace_back(ContourData{0});
			loadArray(c, val);
		} else if (val.isDictionary()) {
			auto ncontours = val.getInteger("ncontours");
			if (val.isInteger("winding")) {
				_winding = vg::Winding(val.getInteger("winding"));
			}
			if (val.isInteger("drawStyle")) {
				_drawStyle = vg::DrawStyle(val.getInteger("drawStyle"));
			}
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

void VgTessCanvas::onEnter(Scene *scene) {
	Node::onEnter(scene);

	_pointerInWindow = _director->getView()->isPointerWithinWindow();
}

void VgTessCanvas::onContentSizeDirty() {
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

void VgTessCanvas::setWinding(vg::Winding w) {
	if (w != _winding) {
		_winding = w;
		updatePoints();
	}
}

void VgTessCanvas::setDrawStyle(vg::DrawStyle s) {
	if (_drawStyle != s) {
		_drawStyle = s;
		updatePoints();
	}
}

void VgTessCanvas::setSelectedContour(uint32_t n) {
	_contourSelected = n % _contours.size();
	_onContourUpdated();
}

uint32_t VgTessCanvas::getSelectedContour() const {
	return _contourSelected;
}

uint32_t VgTessCanvas::getContoursCount() const {
	return _contours.size();
}

void VgTessCanvas::addContour() {
	if (_contours.back().points.size() > 0) {
		_contours.emplace_back(ContourData{uint32_t(_contours.size())});
		_contourSelected = _contours.size() - 1;
		_onContourUpdated();
	}
}

void VgTessCanvas::onTouch(const InputEvent &ev) {
	switch (ev.data.event) {
	case InputEventName::Begin:
		if ((ev.data.modifiers & InputModifier::Ctrl) == InputModifier::None) {
			if (auto pt = getTouchedPoint(ev.currentLocation)) {
				_capturedPoint = pt;
			}
		}
		break;
	case InputEventName::Move:
		if (_capturedPoint) {
			auto loc = convertToNodeSpace(ev.currentLocation);
			loc = Vec2(roundf(loc.x), roundf(loc.y));
			_capturedPoint->setPoint(loc);
			updatePoints();
		}
		break;
	case InputEventName::End:
		if (_capturedPoint) {

		} else if (ev.currentLocation.distance(ev.originalLocation) < TapDistanceAllowed
				&& (ev.currentTime - ev.originalTime) < TapIntervalAllowed.toMicros()) {
			onActionTouch(ev);
		} else {
		}
		_capturedPoint = nullptr;
		break;
	case InputEventName::Cancel:
		_capturedPoint = nullptr;
		break;
	default:
		break;
	}
}

void VgTessCanvas::onMouseMove(const InputEvent &ev) {
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
					_cursor->setState(VgTessCursor::Capture);
					_cursor->setPosition(pos);
				} else {
					_cursor->setState(VgTessCursor::Point);
				}
			}
		} else {
			_cursor->setVisible(false);
		}
	}
}

bool VgTessCanvas::onPointerEnter(bool value) {
	_pointerInWindow = value;
	if (_cursor) {
		_cursor->setVisible(_pointerInWindow && isTouchedNodeSpace(_currentLocation));
	}
	return true;
}

void VgTessCanvas::onActionTouch(const InputEvent &ev) {
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
		auto pt = Rc<VgTessPoint>::create(loc, c.points.size());
		pt->setColor(getColorForIndex(_contourSelected));
		c.points.emplace_back(addChild(move(pt), 10));
		updatePoints();
	}
}

VgTessPoint * VgTessCanvas::getTouchedPoint(const Vec2 &pt) const {
	for (auto &c : _contours) {
		for (auto &it : c.points) {
			if (it->isTouched(pt, 10)) {
				return it;
			}
		}
	}
	return nullptr;
}

void VgTessCanvas::updatePoints() {
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

	saveData();
}

void VgTessCanvas::saveData() {
	auto path = filesystem::writablePath<Interface>("path.cbor");
	filesystem::remove(path);

	Value val;
	val.setInteger(_contours.size(), "ncountours");
	val.setInteger(toInt(_winding), "winding");
	val.setInteger(toInt(_drawStyle), "drawStyle");
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
