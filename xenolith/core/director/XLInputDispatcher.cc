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

#include "XLInputDispatcher.h"

namespace stappler::xenolith {

void InputListenerStorage::addListener(InputListener *input) {
	auto p = input->getPriority();
	if (p == 0) {
		_sceneEvents.emplace_back(input);
	} else if (p < 0) {
		auto lb = std::lower_bound(_postSceneEvents.begin(), _postSceneEvents.end(), input,
				[] (InputListener *l, InputListener *r) {
			return l->getPriority() < r->getPriority();
		});

		if (lb == _postSceneEvents.end()) {
			_postSceneEvents.emplace_back(input);
		} else {
			_postSceneEvents.emplace(lb, input);
		}
	} else {
		auto lb = std::lower_bound(_preSceneEvents.begin(), _preSceneEvents.end(), input,
				[] (InputListener *l, InputListener *r) {
			return l->getPriority() < r->getPriority();
		});

		if (lb == _preSceneEvents.end()) {
			_preSceneEvents.emplace_back(input);
		} else {
			_preSceneEvents.emplace(lb, input);
		}
	}
}

bool InputDispatcher::init(gl::View *view) {
	_textInput = Rc<TextInputManager>::create(view);
	return true;
}

void InputDispatcher::update(const UpdateTime &time) {
	_currentTime = time.global;
}

Rc<InputListenerStorage> InputDispatcher::acquireNewStorage() {
	Rc<InputListenerStorage> req;
	if (_tmpEvents) {
		req = move(_tmpEvents);
		_tmpEvents = nullptr;
	} else {
		req = Rc<InputListenerStorage>::alloc();
	}
	if (_events) {
		req->_postSceneEvents.reserve(_events->_postSceneEvents.size());
		req->_preSceneEvents.reserve(_events->_preSceneEvents.size());
		req->_sceneEvents.reserve(_events->_sceneEvents.size());
	}
	return req;
}

void InputDispatcher::commitStorage(Rc<InputListenerStorage> &&storage) {
	_tmpEvents = move(_events);
	_events = move(storage);

	if (_tmpEvents) {
		_tmpEvents->_postSceneEvents.clear();
		_tmpEvents->_preSceneEvents.clear();
		_tmpEvents->_sceneEvents.clear();
	}
}

void InputDispatcher::handleInputEvent(const InputEventData &event) {
	if (!_events) {
		return;
	}

	switch (event.event) {
	case InputEventName::None:
	case InputEventName::Max:
		return;
		break;
	case InputEventName::Begin: {
		auto v = _activeEvents.find(event.id);
		if (v == _activeEvents.end()) {
			v = _activeEvents.emplace(event.id, EventHandlersInfo{getEventInfo(event), Vector<Rc<InputListener>>()}).first;
		} else {
			v->second.clear(true);
			v->second.event = getEventInfo(event);
		}

		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(v->second.event)) {
				v->second.listeners.emplace_back(l);
				if (l->shouldSwallowEvent(v->second.event)) {
					v->second.listeners.clear();
					v->second.listeners.emplace_back(l);
					return false;
				}
			}
			return true;
		});

		v->second.handle();
		break;
	}
	case InputEventName::Move: {
		auto v = _activeEvents.find(event.id);
		if (v != _activeEvents.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle();
		}
		break;
	}
	case InputEventName::End:
	case InputEventName::Cancel: {
		auto v = _activeEvents.find(event.id);
		if (v != _activeEvents.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle();
			v->second.clear(false);
			_activeEvents.erase(v);
		}
		break;
	}
	case InputEventName::MouseMove: {
		auto ev = getEventInfo(event);
		Vector<InputListener *> listeners;
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(ev)) {
				listeners.emplace_back(l);
				if (l->shouldSwallowEvent(ev)) {
					listeners.clear();
					listeners.emplace_back(l);
					return false;
				}
			}
			return true;
		});
		for (auto &it : listeners) {
			it->handleEvent(ev);
		}
		for (auto &it : _activeEvents) {
			it.second.event.data.x = event.x;
			it.second.event.data.y = event.y;
			it.second.event.data.event = InputEventName::Move;
			it.second.event.data.modifiers = event.modifiers;
			handleInputEvent(it.second.event.data);
		}
		break;
	}
	case InputEventName::Scroll: {
		auto ev = getEventInfo(event);
		Vector<InputListener *> listeners;
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(ev)) {
				listeners.emplace_back(l);
				if (l->shouldSwallowEvent(ev)) {
					listeners.clear();
					listeners.emplace_back(l);
					return false;
				}
			}
			return true;
		});
		for (auto &it : listeners) {
			it->handleEvent(ev);
		}
		break;
	}
	case InputEventName::Background:
	case InputEventName::FocusGain: {
		auto ev = getEventInfo(event);
		Vector<InputListener *> listeners;
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(ev)) {
				listeners.emplace_back(l);
				if (l->shouldSwallowEvent(ev)) {
					listeners.clear();
					listeners.emplace_back(l);
					return false;
				}
			}
			return true;
		});
		for (auto &it : listeners) {
			it->handleEvent(ev);
		}
		break;
	}
	case InputEventName::PointerEnter: {
		auto ev = getEventInfo(event);
		Vector<InputListener *> listeners;
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(ev)) {
				listeners.emplace_back(l);
				if (l->shouldSwallowEvent(ev)) {
					listeners.clear();
					listeners.emplace_back(l);
					return false;
				}
			}
			return true;
		});
		for (auto &it : listeners) {
			it->handleEvent(ev);
		}
		if (!ev.data.getValue()) {
			// Mouse left window
			auto tmpEvents = _activeEvents;
			for (auto &it : tmpEvents) {
				it.second.event.data.x = event.x;
				it.second.event.data.y = event.y;
				it.second.event.data.event = InputEventName::Cancel;
				it.second.event.data.modifiers = event.modifiers;
				handleInputEvent(it.second.event.data);
			}
			_activeEvents.clear();
		}
		break;
	}
	case InputEventName::KeyPressed: {
		if (_textInput->canHandleInputEvent(event)) {
			// forward to text input
			if (_textInput->handleInputEvent(event)) {
				auto v = _activeKeys.find(event.key.keycode);
				if (v != _activeKeys.end()) {
					v->second.clear(true);
					_activeKeys.erase(v);
				}
				return;
			}
		}

		auto v = _activeKeys.find(event.key.keycode);
		if (v == _activeKeys.end()) {
			v = _activeKeys.emplace(event.key.keycode, EventHandlersInfo{getEventInfo(event), Vector<Rc<InputListener>>()}).first;
		} else {
			v->second.clear(true);
			v->second.event = getEventInfo(event);
		}

		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(v->second.event)) {
				v->second.listeners.emplace_back(l);
				if (l->shouldSwallowEvent(v->second.event)) {
					v->second.listeners.clear();
					v->second.listeners.emplace_back(l);
					return false;
				}
			}
			return true;
		});
		v->second.handle();
		break;
	}
	case InputEventName::KeyRepeated: {
		if (_textInput->canHandleInputEvent(event)) {
			// forward to text input
			if (_textInput->handleInputEvent(event)) {
				auto v = _activeKeys.find(event.key.keycode);
				if (v != _activeKeys.end()) {
					v->second.clear(true);
					_activeKeys.erase(v);
				}
				return;
			}
		}

		auto v = _activeKeys.find(event.key.keycode);
		if (v != _activeKeys.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle();
		}
		break;
	}
	case InputEventName::KeyReleased:
	case InputEventName::KeyCanceled: {
		if (_textInput->canHandleInputEvent(event)) {
			// forward to text input
			if (_textInput->handleInputEvent(event)) {
				auto v = _activeKeys.find(event.key.keycode);
				if (v != _activeKeys.end()) {
					v->second.clear(true);
					_activeKeys.erase(v);
				}
				return;
			}
		}

		auto v = _activeKeys.find(event.key.keycode);
		if (v != _activeKeys.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle();
			v->second.clear(false);
			_activeKeys.erase(v);
		}

		break;
	}
	}
}

Vector<InputEventData> InputDispatcher::getActiveEvents() const {
	Vector<InputEventData> eventsTmp; eventsTmp.reserve(_activeEvents.size());
	for (auto &it : _activeEvents) {
		eventsTmp.emplace_back(it.second.event.data);
	}
	return eventsTmp;
}

void InputDispatcher::setListenerExclusive(const InputListener *l) {
	for (auto &it : _activeEvents) {
		setListenerExclusive(it.second, l);
	}
	for (auto &it : _activeKeys) {
		setListenerExclusive(it.second, l);
	}
}

void InputDispatcher::setListenerExclusiveForTouch(const InputListener *l, uint32_t id) {
	auto it = _activeEvents.find(id);
	if (it != _activeEvents.end()) {
		setListenerExclusive(it->second, l);
	}
}

void InputDispatcher::setListenerExclusiveForKey(const InputListener *l, InputKeyCode id) {
	auto it = _activeKeys.find(id);
	if (it != _activeKeys.end()) {
		setListenerExclusive(it->second, l);
	}
}

InputEvent InputDispatcher::getEventInfo(const InputEventData &event) const {
	auto loc = Vec2(event.x, event.y);
	return InputEvent{event, loc, loc, loc, _currentTime, _currentTime, _currentTime, event.modifiers, event.modifiers};
}

void InputDispatcher::updateEventInfo(InputEvent &event, const InputEventData &data) const {
	event.previousLocation = event.currentLocation;
	event.currentLocation = Vec2(data.x, data.y);

	event.previousTime = event.currentTime;
	event.currentTime = _currentTime;

	event.previousModifiers = event.data.modifiers;

	event.data.event = data.event;
	event.data.x = data.x;
	event.data.y = data.y;
	event.data.button = data.button;
	event.data.modifiers = data.modifiers;

	if (event.data.isPointEvent()) {
		event.data.point.valueX = data.point.valueX;
		event.data.point.valueY = data.point.valueY;
		event.data.point.density = data.point.density;
	} else if (event.data.isKeyEvent()) {
		event.data.key.keychar = data.key.keychar;
		event.data.key.keycode = data.key.keycode;
		event.data.key.keysym = data.key.keysym;
	}
}

void InputDispatcher::EventHandlersInfo::handle() {
	if (exclusive) {
		current = exclusive.get();
		exclusive->handleEvent(event);
		current = nullptr;
	} else {
		auto vec = listeners;
		for (auto &it : vec) {
			current = it.get();
			it->handleEvent(event);
			current = nullptr;

			if (exclusive) {
				if (!exclusiveDirty) {
					current = exclusive.get();
					exclusive->handleEvent(event);
					current = nullptr;
				} else {
					exclusiveDirty = true;
				}
				break;
			}
		}
	}
}

void InputDispatcher::EventHandlersInfo::clear(bool cancel) {
	if (cancel) {
		event.data.event = InputEventName::Cancel;
		handle();
	}

	listeners.clear();
	exclusive = nullptr;
}

void InputDispatcher::setListenerExclusive(EventHandlersInfo &info, const InputListener *l) const {
	if (info.exclusive) {
		return;
	}

	auto v = std::find(info.listeners.begin(), info.listeners.end(), l);
	if (v != info.listeners.end()) {
		info.exclusive = *v;
		info.exclusiveDirty = info.current != l;

		auto event = info.event;
		event.data.event = InputEventName::Cancel;
		for (auto &iit : info.listeners) {
			if (iit.get() != l) {
				iit->handleEvent(event);
			}
		}
		info.listeners.clear();
	}
}

}
