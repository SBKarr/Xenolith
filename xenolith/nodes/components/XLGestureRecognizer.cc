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

#include "XLGestureRecognizer.h"

namespace stappler::xenolith {

const Vec2 &GestureScroll::location() const {
	return pos;
}

void GestureScroll::cleanup() {
	pos = Vec2::ZERO;
	amount = Vec2::ZERO;
}

bool GestureRecognizer::init() {
	return true;
}

bool GestureRecognizer::canHandleEvent(const InputEvent &event) const {
	if (_eventMask.test(toInt(event.data.event))) {
		if (!_buttonMask.any() || _buttonMask.test(toInt(event.data.button))) {
			return true;
		}
	}
	return false;
}

bool GestureRecognizer::handleInputEvent(const InputEvent &event) {
	if (!_eventMask.test(toInt(event.data.event))) {
		return false;
	}

	if (_buttonMask.any() && !_buttonMask.test(toInt(event.data.button))) {
		return false;
	}

	switch (event.data.event) {
	case InputEventName::Begin:
	case InputEventName::KeyPressed:
		return addEvent(event);
		break;
	case InputEventName::Move:
	case InputEventName::KeyRepeated:
		renewEvent(event);
		break;
	case InputEventName::End:
	case InputEventName::KeyReleased:
		removeEvent(event, true);
		break;
	case InputEventName::Cancel:
	case InputEventName::KeyCanceled:
		removeEvent(event, false);
		break;
	default: break;
	}
	return true;
}

uint32_t GestureRecognizer::getEventCount() const {
	return (uint32_t)_events.size();
}

bool GestureRecognizer::hasEvent(const InputEvent &event) const {
	if (_events.size() == 0) {
		return false;
	}

	for (auto & pEvent : _events) {
		if (pEvent.data.id == event.data.id) {
			return true;
		}
	}

	return false;
}

GestureEvent GestureRecognizer::getEvent() const {
	return _event;
}

GestureRecognizer::EventMask GestureRecognizer::getEventMask() const {
	return _eventMask;
}

void GestureRecognizer::update(uint64_t dt) {

}

Vec2 GestureRecognizer::getLocation() const {
	if (_events.size() > 0) {
		return Vec2(_events.back().currentLocation);
	} else {
		return Vec2::ZERO;
	}
}

void GestureRecognizer::cancel() {
	auto eventsToRemove = _events;
	for (auto &event : eventsToRemove) {
		removeEvent(event, false);
	}
}

bool GestureRecognizer::addEvent(const InputEvent &event) {
	if (_events.size() < _maxEvents) {
		for (auto &it : _events) {
			if (event.data.id == it.data.id) {
				return false;
			}
		}
		_events.emplace_back(event);
		return true;
	} else {
		return false;
	}
}

bool GestureRecognizer::removeEvent(const InputEvent &event, bool success) {
	if (_events.size() == 0) {
		return false;
	}

	uint32_t index = maxOf<uint32_t>();
	auto pEvent = getTouchById(event.data.id, &index);
	if (pEvent && index < _events.size()) {
		_events.erase(_events.begin() + index);
		return true;
	} else {
		return false;
	}
}

bool GestureRecognizer::renewEvent(const InputEvent &event) {
	if (_events.size() == 0) {
		return false;
	}

	uint32_t index = maxOf<uint32_t>();
	auto pEvent = getTouchById(event.data.id, &index);
	if (pEvent && index < _events.size()) {
		_events[index] = event;
		return true;
	} else {
		return false;
	}
}

InputEvent *GestureRecognizer::getTouchById(uint32_t id, uint32_t *index) {
	InputEvent *pTouch = nullptr; uint32_t i = 0;
	for (i = 0; i < _events.size(); i ++) {
		pTouch = &_events.at(i);
		if (pTouch->data.id == id) {
			if (index) {
				*index = i;
			}
			return pTouch;
		}
	}
	return nullptr;
}

bool GestureTouchRecognizer::init(InputCallback &&cb, ButtonMask &&mask) {
	if (cb) {
		_maxEvents = 10;
		_buttonMask = move(mask);
		_callback = move(cb);
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

bool GestureTouchRecognizer::canHandleEvent(const InputEvent &event) const {
	if (GestureRecognizer::canHandleEvent(event)) {
		if (_buttonMask.test(toInt(event.data.button))) {
			return true;
		}
	}
	return false;
}

void GestureTouchRecognizer::removeRecognizedEvent(uint32_t id) {
	for (auto it = _events.begin(); it != _events.end(); it ++) {
		if ((*it).data.id == id) {
			_events.erase(it);
			break;
		}
	}
}

bool GestureTouchRecognizer::addEvent(const InputEvent &event) {
	if (!_buttonMask.test(toInt(event.data.button))) {
		return false;
	}

	if (GestureRecognizer::addEvent(event)) {
		_event = GestureEvent::Began;
		if (!_callback(_event, event)) {
			removeRecognizedEvent(event.data.id);
			_event = GestureEvent::Cancelled;
			return false;
		}
		return true;
	}
	return false;
}

bool GestureTouchRecognizer::removeEvent(const InputEvent &event, bool successful) {
	if (GestureRecognizer::removeEvent(event, successful)) {
		_event = successful ? GestureEvent::Ended : GestureEvent::Cancelled;
		_callback(_event, event);
		_event = GestureEvent::Cancelled;
		return true;
	}
	return false;
}

bool GestureTouchRecognizer::renewEvent(const InputEvent &event) {
	if (GestureRecognizer::renewEvent(event)) {
		_event = GestureEvent::Activated;
		if (!_callback(_event, event)) {
			removeRecognizedEvent(event.data.id);
			_event = GestureEvent::Cancelled;
		}
		return true;
	}
	return false;
}


bool GestureTapRecognizer::init(InputCallback &&cb, ButtonMask &&mask) {
	if (cb) {
		_maxEvents = 1;
		_callback = move(cb);
		_buttonMask = move(mask);
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

void GestureTapRecognizer::update(uint64_t dt) {
	GestureRecognizer::update(dt);

	auto now = Time::now();
	if (_gesture.count > 0 && _gesture.time - now > TapIntervalAllowed * _gesture.density) {
		_event = GestureEvent::Activated;
		if (_callback) {
			_callback(_event, _gesture);
		}
		_event = GestureEvent::Cancelled;
		_gesture.cleanup();
	}
}

void GestureTapRecognizer::cancel() {
	_gesture.cleanup();
}

bool GestureTapRecognizer::addEvent(const InputEvent &ev) {
	if (_gesture.count > 0 && _gesture.pos.getDistance(ev.currentLocation) > TapDistanceAllowedMulti * _gesture.density) {
		return false;
	}
	if (GestureRecognizer::addEvent(ev)) {
		auto count = _gesture.count;
		auto time = _gesture.time;
		_gesture.cleanup();
		if (time - Time::now() < TapIntervalAllowed) {
			_gesture.count = count;
			_gesture.time = time;
		}
		_gesture.pos = ev.currentLocation;
		return true;
	}
	return false;
}

bool GestureTapRecognizer::removeEvent(const InputEvent &ev, bool successful) {
	bool ret = false;
	if (GestureRecognizer::removeEvent(ev, successful)) {
		if (successful && _gesture.pos.getDistance(ev.currentLocation) <= TapDistanceAllowed) {
			registerTap();
		}
		ret = true;
	}
	return ret;
}

bool GestureTapRecognizer::renewEvent(const InputEvent &ev) {
	if (GestureRecognizer::renewEvent(ev)) {
		if (_gesture.pos.getDistance(ev.currentLocation) > TapDistanceAllowed) {
			return removeEvent(ev, false);
		}
	}
	return false;
}

void GestureTapRecognizer::registerTap() {
	auto currentTime = Time::now();

	if (currentTime < _gesture.time + TapIntervalAllowed) {
		_gesture.count ++;
	} else {
		_gesture.count = 1;
	}

	_gesture.time = currentTime;
	if (_gesture.count == 2) {
		_event = GestureEvent::Activated;
		if (_callback) {
			_callback(_event, _gesture);
		}
		_event = GestureEvent::Cancelled;
		_gesture.cleanup();
	}
}


bool GestureScrollRecognizer::init(InputCallback &&cb) {
	if (cb) {
		_callback = move(cb);
		_eventMask.set(toInt(InputEventName::Scroll));
		return true;
	}

	return false;
}

bool GestureScrollRecognizer::handleInputEvent(const InputEvent &event) {
	if (!_eventMask.test(toInt(event.data.event))) {
		return false;
	}

	_gesture.pos = event.currentLocation;
	_gesture.amount = Vec2(event.data.point.valueX, event.data.point.valueY);
	_event = GestureEvent::Activated;
	if (_callback) {
		_callback(_event, _gesture);
	}
	_event = GestureEvent::Cancelled;
	return true;
}

bool GestureMoveRecognizer::init(InputCallback &&cb) {
	if (cb) {
		_callback = move(cb);
		_eventMask.set(toInt(InputEventName::MouseMove));
		return true;
	}

	return false;
}

bool GestureMoveRecognizer::handleInputEvent(const InputEvent &event) {
	if (!_eventMask.test(toInt(event.data.event))) {
		return false;
	}

	_event = GestureEvent::Activated;
	if (_callback) {
		_callback(_event, event);
	}
	_event = GestureEvent::Cancelled;
	return true;
}

bool GestureKeyRecognizer::init(InputCallback &&cb, KeyMask &&mask) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb && mask.any()) {
		_callback = move(cb);
		_keyMask = mask;
		_eventMask.set(toInt(InputEventName::KeyPressed));
		_eventMask.set(toInt(InputEventName::KeyRepeated));
		_eventMask.set(toInt(InputEventName::KeyReleased));
		_eventMask.set(toInt(InputEventName::KeyCanceled));
		return true;
	}

	log::text("GestureKeyRecognizer", "Callback or key mask is not defined");
	return false;
}

bool GestureKeyRecognizer::canHandleEvent(const InputEvent &ev) const {
	if (GestureRecognizer::canHandleEvent(ev)) {
		if (_keyMask.test(toInt(ev.data.key.keycode))) {
			return true;
		}
	}
	return false;
}

bool GestureKeyRecognizer::addEvent(const InputEvent &event) {
	_pressedKeys.set(toInt(event.data.key.keycode));
	_callback(GestureEvent::Began, event);
	return true;
}

bool GestureKeyRecognizer::removeEvent(const InputEvent &event, bool success) {
	if (_pressedKeys.test(toInt(event.data.key.keycode))) {
		_callback(success ? GestureEvent::Ended : GestureEvent::Cancelled, event);
		_pressedKeys.reset(toInt(event.data.key.keycode));
		return true;
	}
	return false;
}

bool GestureKeyRecognizer::renewEvent(const InputEvent &event) {
	if (_pressedKeys.test(toInt(event.data.key.keycode))) {
		_callback(GestureEvent::Activated, event);
		return true;
	}
	return false;
}

std::ostream &operator<<(std::ostream &stream, GestureEvent ev) {
	switch (ev) {
	case GestureEvent::Began: stream << "GestureEvent::Began"; break;
	case GestureEvent::Activated: stream << "GestureEvent::Activated"; break;
	case GestureEvent::Ended: stream << "GestureEvent::Ended"; break;
	case GestureEvent::Cancelled: stream << "GestureEvent::Cancelled"; break;
	}
	return stream;
}

}
