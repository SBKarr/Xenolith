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

#include "XLInputListener.h"
#include "XLScene.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"
#include "XLGestureRecognizer.h"
#include "XLScheduler.h"

namespace stappler::xenolith {

InputListener::ButtonMask InputListener::makeButtonMask(std::initializer_list<InputMouseButton> &&il) {
	ButtonMask ret;
	for (auto &it : il) {
		ret.set(toInt(it));
	}
	return ret;
}

InputListener::EventMask InputListener::makeEventMask(std::initializer_list<InputEventName> &&il) {
	EventMask ret;
	for (auto &it : il) {
		ret.set(toInt(it));
	}
	return ret;
}

InputListener::KeyMask InputListener::makeKeyMask(std::initializer_list<InputKeyCode> &&il) {
	KeyMask ret;
	for (auto &it : il) {
		ret.set(toInt(it));
	}
	return ret;
}

InputListener::InputListener()
: _enabled(true), _owner(nullptr) { }

InputListener::~InputListener() { }

bool InputListener::init(int32_t priority) {
	_priority = priority;
	return true;
}

void InputListener::onEnter(Scene *scene) {
	_running = true;
	_scene = scene;

	_scene->getDirector()->getScheduler()->scheduleUpdate(this);

	for (auto &it : _recognizers) {
		it->onEnter(this);
	}
}

void InputListener::onExit() {
	for (auto &it : _recognizers) {
		it->onExit();
	}

	_scene->getDirector()->getScheduler()->unschedule(this);
	_running = false;
	_scene = nullptr;
}

void InputListener::update(UpdateTime dt) {
	for (auto &it : _recognizers) {
		it->update(dt.delta);
	}
}

void InputListener::setOwner(Node *owner) {
	_owner = owner;
}

void InputListener::setEnabled(bool b) {
	_enabled = b;
}

void InputListener::setPriority(int32_t p) {
	_priority = p;
}

void InputListener::setExclusive() {
	if (_scene) {
		_scene->getDirector()->getInputDispatcher()->setListenerExclusive(this);
	}
}

void InputListener::setExclusiveForTouch(uint32_t eventId) {
	if (_scene) {
		_scene->getDirector()->getInputDispatcher()->setListenerExclusiveForTouch(this, eventId);
	}
}

void InputListener::setSwallowEvents(EventMask &&mask) {
	_swallowEvents = move(mask);
}

void InputListener::setSwallowAllEvents() {
	_swallowEvents.set();
}

void InputListener::setSwallowEvent(InputEventName event) {
	_swallowEvents.set(toInt(event));
}

void InputListener::clearSwallowAllEvents() {
	_swallowEvents.reset();
}

void InputListener::clearSwallowEvent(InputEventName event) {
	_swallowEvents.reset(toInt(event));
}

void InputListener::setTouchFilter(const EventFilter &filter) {
	_eventFilter = filter;
}

bool InputListener::shouldSwallowEvent(const InputEvent &event) const {
	return _swallowEvents.test(toInt(event.data.event));
}

bool InputListener::canHandleEvent(const InputEvent &event) const {
	if (_eventMask.test(toInt(event.data.event)) && shouldProcessEvent(event)) {
		auto it = _callbacks.find(event.data.event);
		if (it != _callbacks.end()) {
			return true;
		}
		for (auto &it : _recognizers) {
			if (!_running || !_owner) {
				break;
			}

			if (it->canHandleEvent(event)) {
				return true;
			}
		}
	}
	return false;
}

bool InputListener::handleEvent(const InputEvent &event) {
	bool ret = false;
	auto it = _callbacks.find(event.data.event);
	if (it != _callbacks.end()) {
		if (it->second(event.data.getValue())) {
			ret = true;
		}
	}
	for (auto &it : _recognizers) {
		if (!_running || !_owner) {
			break;
		}

		if (it->handleInputEvent(event, _density)) {
			ret = true;
		}
	}
	return ret;
}

GestureRecognizer *InputListener::addTouchRecognizer(InputCallback<GestureData> &&cb, ButtonMask &&buttonMask) {
	return addRecognizer(Rc<GestureTouchRecognizer>::create(move(cb), move(buttonMask)));
}

GestureRecognizer *InputListener::addTapRecognizer(InputCallback<GestureTap> &&cb, ButtonMask &&buttonMask, uint32_t maxTapCount) {
	return addRecognizer(Rc<GestureTapRecognizer>::create(move(cb), move(buttonMask), maxTapCount));
}

GestureRecognizer *InputListener::addScrollRecognizer(InputCallback<GestureScroll> &&cb) {
	return addRecognizer(Rc<GestureScrollRecognizer>::create(move(cb)));
}

GestureRecognizer *InputListener::addPressRecognizer(InputCallback<GesturePress> &&cb, TimeInterval interval, bool continuous,
		ButtonMask &&mask) {
	return addRecognizer(Rc<GesturePressRecognizer>::create(move(cb), interval, continuous, move(mask)));
}

GestureRecognizer *InputListener::addSwipeRecognizer(InputCallback<GestureSwipe> &&cb, float threshold, bool sendThreshold, ButtonMask &&mask) {
	return addRecognizer(Rc<GestureSwipeRecognizer>::create(move(cb), threshold, sendThreshold, move(mask)));
}

GestureRecognizer *InputListener::addMoveRecognizer(InputCallback<GestureData> &&cb, bool withinNode) {
	return addRecognizer(Rc<GestureMoveRecognizer>::create(move(cb), withinNode));
}

GestureRecognizer *InputListener::addMouseOverRecognizer(InputCallback<GestureData> &&cb, float padding) {
	return addRecognizer(Rc<GestureMouseOverRecognizer>::create(move(cb)));
}

GestureKeyRecognizer *InputListener::addKeyRecognizer(InputCallback<GestureData> &&cb, KeyMask &&keys) {
	return (GestureKeyRecognizer *)addRecognizer(Rc<GestureKeyRecognizer>::create(move(cb), move(keys)));
}

void InputListener::setPointerEnterCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::PointerEnter, move(cb));
		_eventMask.set(toInt(InputEventName::PointerEnter));
	} else {
		_callbacks.erase(InputEventName::PointerEnter);
		_eventMask.reset(toInt(InputEventName::PointerEnter));
	}
}

void InputListener::setBackgroudCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::Background, move(cb));
		_eventMask.set(toInt(InputEventName::Background));
	} else {
		_callbacks.erase(InputEventName::Background);
		_eventMask.reset(toInt(InputEventName::Background));
	}
}

void InputListener::setFocusCallback(Function<bool(bool)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::FocusGain, move(cb));
		_eventMask.set(toInt(InputEventName::FocusGain));
	} else {
		_callbacks.erase(InputEventName::FocusGain);
		_eventMask.reset(toInt(InputEventName::FocusGain));
	}
}

void InputListener::clear() {
	_eventMask.reset();
	_recognizers.clear();
}

bool InputListener::shouldProcessEvent(const InputEvent &event) const {
	if (!_eventFilter) {
		return _shouldProcessEvent(event);
	} else {
		return _eventFilter(event, std::bind(&InputListener::_shouldProcessEvent, this, event));
	}
}

bool InputListener::_shouldProcessEvent(const InputEvent &event) const {
	auto node = getOwner();
	if (node && _running) {
		bool visible = node->isVisible();
		auto p = node->getParent();
		while (visible && p) {
			visible = p->isVisible();
			p = p->getParent();
		}
		if (visible && (!event.data.hasLocation() || event.data.event == InputEventName::MouseMove
				|| node->isTouched(event.currentLocation, _touchPadding))
				&& node->getOpacity() >= _opacityFilter) {
			return true;
		}
	}
	return false;
}

void InputListener::addEventMask(const EventMask &mask) {
	for (size_t i = 0; i < mask.size(); ++ i) {
		if (mask.test(i)) {
			_eventMask.set(i);
		}
	}
}

GestureRecognizer *InputListener::addRecognizer(GestureRecognizer *rec) {
	addEventMask(rec->getEventMask());
	auto ret = _recognizers.emplace_back(move(rec)).get();
	if (_running) {
		ret->onEnter(this);
	}
	return ret;
}

}
