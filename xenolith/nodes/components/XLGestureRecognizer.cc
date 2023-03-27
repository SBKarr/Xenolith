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

#include "XLGestureRecognizer.h"
#include "XLInputListener.h"
#include "XLNode.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"

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

bool GestureRecognizer::handleInputEvent(const InputEvent &event, float density) {
	if (!_eventMask.test(toInt(event.data.event))) {
		return false;
	}

	if (_buttonMask.any() && !_buttonMask.test(toInt(event.data.button))) {
		return false;
	}

	_density = density;

	switch (event.data.event) {
	case InputEventName::Begin:
	case InputEventName::KeyPressed:
		return addEvent(event, density);
		break;
	case InputEventName::Move:
	case InputEventName::KeyRepeated:
		renewEvent(event, density);
		break;
	case InputEventName::End:
	case InputEventName::KeyReleased:
		removeEvent(event, true, density);
		break;
	case InputEventName::Cancel:
	case InputEventName::KeyCanceled:
		removeEvent(event, false, density);
		break;
	default: break;
	}
	return true;
}

void GestureRecognizer::onEnter(InputListener *) {

}

void GestureRecognizer::onExit() {

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
		removeEvent(event, false, _density);
	}
}

bool GestureRecognizer::addEvent(const InputEvent &event, float density) {
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

bool GestureRecognizer::removeEvent(const InputEvent &event, bool success, float density) {
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

bool GestureRecognizer::renewEvent(const InputEvent &event, float density) {
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
	if (!GestureRecognizer::init()) {
		return false;
	}

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

bool GestureTouchRecognizer::addEvent(const InputEvent &event, float density) {
	if (!_buttonMask.test(toInt(event.data.button))) {
		return false;
	}

	if (GestureRecognizer::addEvent(event, density)) {
		_event.event = GestureEvent::Began;
		_event.input = &event;
		if (!_callback(_event)) {
			removeRecognizedEvent(event.data.id);
			_event.event = GestureEvent::Cancelled;
			_event.input = nullptr;
			return false;
		}
		return true;
	}
	return false;
}

bool GestureTouchRecognizer::removeEvent(const InputEvent &event, bool successful, float density) {
	if (GestureRecognizer::removeEvent(event, successful, density)) {
		_event.event = successful ? GestureEvent::Ended : GestureEvent::Cancelled;
		_event.input = &event;
		_callback(_event);
		_event.event = GestureEvent::Cancelled;
		_event.input = nullptr;
		return true;
	}
	return false;
}

bool GestureTouchRecognizer::renewEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::renewEvent(event, density)) {
		_event.event = GestureEvent::Activated;
		_event.input = &event;
		if (!_callback(_event)) {
			removeRecognizedEvent(event.data.id);
			_event.event = GestureEvent::Cancelled;
			_event.input = nullptr;
		}
		return true;
	}
	return false;
}


bool GestureTapRecognizer::init(InputCallback &&cb, ButtonMask &&mask, uint32_t maxTapCount) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 1;
		_maxTapCount = maxTapCount;
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
	if (_gesture.count > 0 && _gesture.time - now > TapIntervalAllowed) {
		_gesture.event = GestureEvent::Activated;
		_gesture.input = &_events.front();
		_callback(_gesture);
		_gesture.event = GestureEvent::Cancelled;
		_gesture.input = nullptr;
		_gesture.cleanup();
	}
}

void GestureTapRecognizer::cancel() {
	GestureRecognizer::cancel();
	_gesture.cleanup();
}

bool GestureTapRecognizer::addEvent(const InputEvent &ev, float density) {
	if (_gesture.count > 0 && _gesture.pos.getDistance(ev.currentLocation) > TapDistanceAllowedMulti * density) {
		return false;
	}
	if (GestureRecognizer::addEvent(ev, density)) {
		auto count = _gesture.count;
		auto time = _gesture.time;
		_gesture.cleanup();
		if (time - Time::now() < TapIntervalAllowed) {
			_gesture.count = count;
			_gesture.time = time;
		}
		_gesture.id = ev.data.id;
		_gesture.pos = ev.currentLocation;
		return true;
	}
	return false;
}

bool GestureTapRecognizer::removeEvent(const InputEvent &ev, bool successful, float density) {
	bool ret = false;
	if (GestureRecognizer::removeEvent(ev, successful, density)) {
		if (successful && _gesture.pos.getDistance(ev.currentLocation) <= TapDistanceAllowed * density) {
			registerTap();
		}
		ret = true;
	}
	return ret;
}

bool GestureTapRecognizer::renewEvent(const InputEvent &ev, float density) {
	if (GestureRecognizer::renewEvent(ev, density)) {
		if (_gesture.pos.getDistance(ev.currentLocation) > TapDistanceAllowed * density) {
			return removeEvent(ev, false, density);
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
	if (_gesture.count == _maxTapCount) {
		_gesture.event = GestureEvent::Activated;
		_gesture.input = &_events.front();
		_callback(_gesture);
		_gesture.event = GestureEvent::Cancelled;
		_gesture.input = nullptr;
		_gesture.cleanup();
	}
}

bool GesturePressRecognizer::init(InputCallback &&cb, TimeInterval interval, bool continuous, ButtonMask &&btn) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 1;
		_callback = move(cb);
		_interval = interval;
		_continuous = continuous;
		_buttonMask = move(btn);

		// enable all touch events
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

void GesturePressRecognizer::cancel() {
	GestureRecognizer::cancel();
	_gesture.cleanup();
	_lastTime.clear();
}

void GesturePressRecognizer::update(uint64_t dt) {
	if ((!_notified || _continuous) && _lastTime && _events.size() > 0) {
		auto time = Time::now() - _lastTime;
		if (_gesture.time.mksec() / _interval.mksec() != time.mksec() / _interval.mksec()) {
			_gesture.time = time;
			++ _gesture.tickCount;
			_gesture.event = GestureEvent::Activated;
			_gesture.input = &_events.front();
			if (!_callback(_gesture)) {
				cancel();
			}
			_notified = true;
		}
	}
}

bool GesturePressRecognizer::addEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::addEvent(event, density)) {
		_gesture.cleanup();
		_gesture.pos = event.currentLocation;
		_gesture.time.clear();
		_gesture.limit = _interval;
		_gesture.event = GestureEvent::Began;
		_gesture.input = &event;
		if (!_callback(_gesture)) {
			cancel();
		}
		_lastTime = Time::now();
		_notified = false;
		return true;
	}
	return false;
}

bool GesturePressRecognizer::removeEvent(const InputEvent &event, bool successful, float density) {
	bool ret = false;
	if (GestureRecognizer::removeEvent(event, successful, density)) {
		float distance = event.originalLocation.getDistance(event.currentLocation);
		_gesture.time = Time::now() - _lastTime;
		_gesture.event = (successful && distance <= TapDistanceAllowed * density) ? GestureEvent::Ended : GestureEvent::Cancelled;
		_gesture.input = &event;
		_callback(_gesture);
		_gesture.event = GestureEvent::Cancelled;
		_gesture.input = nullptr;
		_lastTime.clear();
		_gesture.cleanup();
		_notified = true;
		ret = true;
	}
	return ret;
}

bool GesturePressRecognizer::renewEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::renewEvent(event, density)) {
		if (event.originalLocation.getDistance(event.currentLocation) > TapDistanceAllowed * density) {
			return removeEvent(event, false, density);
		}
	}
	return false;
}

bool GestureSwipeRecognizer::init(InputCallback &&cb, float threshold, bool includeThreshold, ButtonMask &&btn) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 2;
		_callback = move(cb);
		_threshold = threshold;
		_includeThreshold = includeThreshold;
		_buttonMask = move(btn);

		// enable all touch events
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

void GestureSwipeRecognizer::cancel() {
	GestureRecognizer::cancel();
	_gesture.cleanup();
	_swipeBegin = false;
	_lastTime.clear();
	_currentTouch = maxOf<uint32_t>();
}

bool GestureSwipeRecognizer::addEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::addEvent(event, density)) {
		Vec2 accum;
		for (auto &it : _events) {
			accum = accum + it.currentLocation;
		}
		accum /= float(_events.size());

		_gesture.midpoint = accum;
		_currentTouch = event.data.id;
		_lastTime = Time::now();
		return true;
	} else {
		return false;
	}
}

bool GestureSwipeRecognizer::removeEvent(const InputEvent &event, bool successful, float density) {
	bool ret = false;
	if (GestureRecognizer::removeEvent(event, successful, density)) {
		if (_events.size() > 0) {
			_currentTouch = _events.back().data.id;
            _lastTime = Time::now();
		} else {
			if (_swipeBegin) {
				_gesture.event = successful ? GestureEvent::Ended : GestureEvent::Cancelled;
				_gesture.input = &event;
				_callback(_gesture);
			}

			_gesture.event = GestureEvent::Cancelled;
			_gesture.input = nullptr;
			_gesture.cleanup();
			_swipeBegin = false;

			_currentTouch = maxOf<uint32_t>();

			_velocityX.dropValues();
			_velocityY.dropValues();

            _lastTime.clear();
		}
		ret = true;
	}
	return ret;
}

bool GestureSwipeRecognizer::renewEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::renewEvent(event, density)) {
		if (_events.size() == 1) {
			Vec2 current = event.currentLocation;
			Vec2 prev = (_swipeBegin)?event.previousLocation:event.originalLocation;

			_gesture.secondTouch = _gesture.firstTouch = current;
			_gesture.midpoint = current;

			_gesture.delta = current - prev;
			_gesture.density = density;

			if (!_swipeBegin && _gesture.delta.length() > _threshold * density) {
				_gesture.cleanup();
				if (_includeThreshold) {
					_gesture.delta = current - prev;
				} else {
					_gesture.delta = current - event.previousLocation;
				}
				_gesture.secondTouch = _gesture.firstTouch = current;
				_gesture.midpoint = current;

				_swipeBegin = true;
				_gesture.event = GestureEvent::Began;
				_gesture.input = &event;
				if (!_callback(_gesture)) {
					cancel();
					return false;
				}
			}

			if (_swipeBegin /* && _gesture.delta.length() > 0.01f */) {
				auto t = Time::now();
				float tm = (float)1000000LL / (float)((t - _lastTime).toMicroseconds());
				if (tm > 80) {
					tm = 80;
				}

				float velX = _velocityX.step(_gesture.delta.x * tm);
				float velY = _velocityY.step(_gesture.delta.y * tm);

				_gesture.velocity = Vec2(velX, velY);
				_gesture.event = GestureEvent::Activated;
				_gesture.input = &event;
				if (!_callback(_gesture)) {
					cancel();
					return false;
				}

				_lastTime = t;
			}
		} else if (_events.size() == 2) {
			Vec2 current = event.currentLocation;
			Vec2 second = _gesture.secondTouch;
			Vec2 prev = _gesture.midpoint;

			_gesture.density = density;

			if (event.data.id != _currentTouch) {
				second = _gesture.secondTouch = current;
			} else if (event.data.id == _currentTouch) {
				_gesture.firstTouch = current;
				_gesture.midpoint = _gesture.secondTouch.getMidpoint(_gesture.firstTouch);
				_gesture.delta = _gesture.midpoint - prev;

				if (!_swipeBegin && _gesture.delta.length() > _threshold * density) {
					_gesture.cleanup();
					_gesture.firstTouch = current;
					_gesture.secondTouch = current;
					_gesture.midpoint = _gesture.secondTouch.getMidpoint(_gesture.firstTouch);
					_gesture.delta = _gesture.midpoint - prev;

					_swipeBegin = true;
					_gesture.event = GestureEvent::Began;
					_gesture.input = &event;
					if (!_callback(_gesture)) {
						cancel();
						return false;
					}
				}

				if (_swipeBegin /* && _gesture.delta.length() > 0.01f */ ) {
					auto t = Time::now();
					float tm = (float)1000000LL / (float)((t - _lastTime).toMicroseconds());
					if (tm > 80) {
						tm = 80;
					}

					float velX = _velocityX.step(_gesture.delta.x * tm);
					float velY = _velocityY.step(_gesture.delta.y * tm);

					_gesture.velocity = Vec2(velX, velY);
					_gesture.event = GestureEvent::Activated;
					_gesture.input = &event;
					if (!_callback(_gesture)) {
						cancel();
						return false;
					}

					_lastTime = t;
				}
			}
		}
		return true;
	} else {
		return false;
	}
}

bool GesturePinchRecognizer::init(InputCallback &&cb, ButtonMask &&btn) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 2;
		_callback = move(cb);
		_buttonMask = move(btn);

		// enable all touch events
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

void GesturePinchRecognizer::cancel() {
	GestureRecognizer::cancel();
	_gesture.cleanup();
	_velocity.dropValues();
	_lastTime.clear();
}

bool GesturePinchRecognizer::addEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::addEvent(event, density)) {
		if (_events.size() == 2) {
			_gesture.cleanup();
			_gesture.first = _events.at(0).currentLocation;
			_gesture.second = _events.at(1).currentLocation;
			_gesture.center = _gesture.first.getMidpoint(_gesture.second);
			_gesture.distance = _gesture.prevDistance = _gesture.startDistance = _gesture.first.getDistance(_gesture.second);
			_gesture.scale = _gesture.distance / _gesture.startDistance;
			_gesture.event = GestureEvent::Began;
			_gesture.input = &_events.at(0);
			_gesture.density = density;
			_lastTime = Time::now();
			if (_callback) {
				_callback(_gesture);
			}
		}
		return true;
	} else {
		return false;
	}
}

bool GesturePinchRecognizer::removeEvent(const InputEvent &event, bool successful, float density) {
	bool ret = false;
	if (GestureRecognizer::removeEvent(event, successful, density)) {
		if (_events.size() == 1) {
			_gesture.event = successful ? GestureEvent::Ended : GestureEvent::Cancelled;
			if (_callback) {
				_callback(_gesture);
			}
            _gesture.cleanup();
			_lastTime.clear();
			_velocity.dropValues();
		}
		ret = true;
	}
	return ret;
}

bool GesturePinchRecognizer::renewEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::renewEvent(event, density)) {
		if (_events.size() == 2) {
			auto &first = _events.at(0);
			auto &second = _events.at(1);
			if (event.data.id == first.data.id || event.data.id == second.data.id) {
				auto scale = _gesture.scale;

				_gesture.first = _events.at(0).currentLocation;
				_gesture.second = _events.at(1).currentLocation;
				_gesture.center = _gesture.first.getMidpoint(_gesture.second);
				_gesture.prevDistance = _events.at(0).previousLocation.getDistance(_events.at(1).previousLocation);
				_gesture.distance = _gesture.first.getDistance(_gesture.second);
				_gesture.scale = _gesture.distance / _gesture.startDistance;
				_gesture.density = density;

				auto t = Time::now();
				float tm = (float)1000000LL / (float)((t - _lastTime).toMicroseconds());
				if (tm > 80) {
					tm = 80;
				}
				_velocity.addValue((scale - _gesture.scale) * tm);
				_gesture.velocity = _velocity.getAverage();

				_gesture.event = GestureEvent::Activated;
				if (_callback) {
					_callback(_gesture);
				}

				_lastTime = t;
			}
		}
	}
	return false;
}

bool GestureScrollRecognizer::init(InputCallback &&cb) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_callback = move(cb);
		_eventMask.set(toInt(InputEventName::Scroll));
		return true;
	}

	return false;
}

bool GestureScrollRecognizer::handleInputEvent(const InputEvent &event, float density) {
	if (!_eventMask.test(toInt(event.data.event))) {
		return false;
	}

	_gesture.event = GestureEvent::Activated;
	_gesture.input = &event;
	_gesture.pos = event.currentLocation;
	_gesture.amount = Vec2(event.data.point.valueX, event.data.point.valueY);
	if (_callback) {
		_callback(_gesture);
	}
	_gesture.event = GestureEvent::Cancelled;
	return true;
}


bool GestureMoveRecognizer::init(InputCallback &&cb, bool withinNode) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_callback = move(cb);
		_eventMask.set(toInt(InputEventName::MouseMove));
		_onlyWithinNode = withinNode;
		return true;
	}

	return false;
}

bool GestureMoveRecognizer::canHandleEvent(const InputEvent &event) const {
	if (GestureRecognizer::canHandleEvent(event)) {
		if (!_onlyWithinNode || (_listener && _listener->getOwner()
				&& _listener->getOwner()->isTouched(event.currentLocation, _listener->getTouchPadding()))) {
			return true;
		}
	}
	return false;
}

bool GestureMoveRecognizer::handleInputEvent(const InputEvent &event, float density) {
	if (!canHandleEvent(event)) {
		return false;
	}

	if (!_eventMask.test(toInt(event.data.event))) {
		return false;
	}

	_event.event = GestureEvent::Activated;
	_event.input = &event;
	if (_callback) {
		_callback(_event);
	}
	_event.input = nullptr;
	_event.event = GestureEvent::Cancelled;
	return true;
}

void GestureMoveRecognizer::onEnter(InputListener *l) {
	GestureRecognizer::onEnter(l);
	_listener = l;
}

void GestureMoveRecognizer::onExit() {
	_listener = nullptr;
	GestureRecognizer::onExit();
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

bool GestureKeyRecognizer::isKeyPressed(InputKeyCode code) const {
	if (_pressedKeys.test(toInt(code))) {
		return true;
	}
	return false;
}

bool GestureKeyRecognizer::addEvent(const InputEvent &event, float density) {
	if (_keyMask.test(toInt(event.data.key.keycode))) {
		_pressedKeys.set(toInt(event.data.key.keycode));
		return _callback(GestureData{GestureEvent::Began, &event});
	}
	return false;
}

bool GestureKeyRecognizer::removeEvent(const InputEvent &event, bool success, float density) {
	if (_pressedKeys.test(toInt(event.data.key.keycode))) {
		_callback(GestureData{success ? GestureEvent::Ended : GestureEvent::Cancelled, &event});
		_pressedKeys.reset(toInt(event.data.key.keycode));
		return true;
	}
	return false;
}

bool GestureKeyRecognizer::renewEvent(const InputEvent &event, float density) {
	if (_pressedKeys.test(toInt(event.data.key.keycode))) {
		_callback(GestureData{GestureEvent::Activated, &event});
		return true;
	}
	return false;
}

bool GestureMouseOverRecognizer::init(InputCallback &&cb, float padding) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_callback = move(cb);
		_eventMask.set(toInt(InputEventName::MouseMove));
		_eventMask.set(toInt(InputEventName::FocusGain));
		_eventMask.set(toInt(InputEventName::PointerEnter));
		return true;
	}

	log::text("GestureKeyRecognizer", "Callback or key mask is not defined");
	return false;
}

bool GestureMouseOverRecognizer::handleInputEvent(const InputEvent &event, float density) {
	switch (event.data.event) {
	case InputEventName::FocusGain:
		if (_viewHasFocus != event.data.getValue()) {
			_viewHasFocus = event.data.getValue();
			updateState(event);
		}
		break;
	case InputEventName::PointerEnter:
		if (_viewHasPointer != event.data.getValue()) {
			_viewHasPointer = event.data.getValue();
			updateState(event);
		}
		break;
	case InputEventName::MouseMove:
		if (auto tar = _listener->getOwner()) {
			auto v = tar->isTouched(event.currentLocation, _padding);
			if (_hasMouseOver != v) {
				_hasMouseOver = v;
				updateState(event);
			}
		} else {
			if (_hasMouseOver) {
				_hasMouseOver = false;
				updateState(event);
			}
		}
		break;
	default:
		break;
	}
	return false;
}

void GestureMouseOverRecognizer::onEnter(InputListener *l) {
	GestureRecognizer::onEnter(l);
	_listener = l;

	auto dispatcher = l->getOwner()->getDirector()->getInputDispatcher();

	_viewHasPointer = dispatcher->isPointerWithinWindow();
	_viewHasFocus = dispatcher->hasFocus();
}

void GestureMouseOverRecognizer::onExit() {
	_listener = nullptr;
	GestureRecognizer::onExit();
}

void GestureMouseOverRecognizer::updateState(const InputEvent &event) {
	auto value = _viewHasFocus && _viewHasPointer && _hasMouseOver;
	if (value != _value) {
		_value = value;
		_event.input = &event;
		_event.event = _value ? GestureEvent::Began : GestureEvent::Ended;
		_callback(_event);
	}
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
