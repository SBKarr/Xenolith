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
			v = _activeEvents.emplace(event.id, pair(getEventInfo(event), Vector<Rc<InputListener>>())).first;
		} else {
			v->second.first.data.event = InputEventName::Cancel;
			for (auto &it : v->second.second) {
				it->handleEvent(v->second.first);
			}

			v->second.first = getEventInfo(event);
			v->second.second.clear();
		}

		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(v->second.first)) {
				v->second.second.emplace_back(l);
				if (l->shouldSwallowEvent(v->second.first)) {
					v->second.second.clear();
					v->second.second.emplace_back(l);
					return false;
				}
			}
			return true;
		});

		for (auto &it : v->second.second) {
			it->handleEvent(v->second.first);
		}

		break;
	}
	case InputEventName::Move: {
		auto v = _activeEvents.find(event.id);
		if (v != _activeEvents.end()) {
			updateEventInfo(v->second.first, event);
			for (auto &it : v->second.second) {
				it->handleEvent(v->second.first);
			}
		}
		break;
	}
	case InputEventName::End:
	case InputEventName::Cancel: {
		auto v = _activeEvents.find(event.id);
		if (v != _activeEvents.end()) {
			updateEventInfo(v->second.first, event);
			for (auto &it : v->second.second) {
				it->handleEvent(v->second.first);
			}

			v->second.second.clear();
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
			it.second.first.data.x = event.x;
			it.second.first.data.y = event.y;
			it.second.first.data.event = InputEventName::Move;
			it.second.first.data.modifiers = event.modifiers;
			handleInputEvent(it.second.first.data);
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
				it.second.first.data.x = event.x;
				it.second.first.data.y = event.y;
				it.second.first.data.event = InputEventName::Cancel;
				it.second.first.data.modifiers = event.modifiers;
				handleInputEvent(it.second.first.data);
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
					v->second.first.data.event = InputEventName::KeyCanceled;
					for (auto &it : v->second.second) {
						it->handleEvent(v->second.first);
					}

					v->second.first = getEventInfo(event);
					v->second.second.clear();
					_activeKeys.erase(v);
				}
				return;
			}
		}

		auto v = _activeKeys.find(event.key.keycode);
		if (v == _activeKeys.end()) {
			v = _activeKeys.emplace(event.key.keycode, pair(getEventInfo(event), Vector<Rc<InputListener>>())).first;
		} else {
			v->second.first.data.event = InputEventName::KeyCanceled;
			for (auto &it : v->second.second) {
				it->handleEvent(v->second.first);
			}

			v->second.first = getEventInfo(event);
			v->second.second.clear();
		}

		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(v->second.first)) {
				v->second.second.emplace_back(l);
				if (l->shouldSwallowEvent(v->second.first)) {
					v->second.second.clear();
					v->second.second.emplace_back(l);
					return false;
				}
			}
			return true;
		});

		for (auto &it : v->second.second) {
			it->handleEvent(v->second.first);
		}

		break;
	}
	case InputEventName::KeyRepeated: {
		if (_textInput->canHandleInputEvent(event)) {
			// forward to text input
			if (_textInput->handleInputEvent(event)) {
				auto v = _activeKeys.find(event.key.keycode);
				if (v != _activeKeys.end()) {
					v->second.first.data.event = InputEventName::KeyCanceled;
					for (auto &it : v->second.second) {
						it->handleEvent(v->second.first);
					}

					v->second.first = getEventInfo(event);
					v->second.second.clear();
					_activeKeys.erase(v);
				}
				return;
			}
		}

		auto v = _activeKeys.find(event.key.keycode);
		if (v != _activeKeys.end()) {
			updateEventInfo(v->second.first, event);
			for (auto &it : v->second.second) {
				it->handleEvent(v->second.first);
			}
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
					v->second.first.data.event = InputEventName::KeyCanceled;
					for (auto &it : v->second.second) {
						it->handleEvent(v->second.first);
					}

					v->second.first = getEventInfo(event);
					v->second.second.clear();
					_activeKeys.erase(v);
				}
				return;
			}
		}

		auto v = _activeKeys.find(event.key.keycode);
		if (v != _activeKeys.end()) {
			updateEventInfo(v->second.first, event);
			for (auto &it : v->second.second) {
				it->handleEvent(v->second.first);
			}

			v->second.second.clear();
			_activeKeys.erase(v);
		}

		break;
	}
	}
}

Vector<InputEventData> InputDispatcher::getActiveEvents() const {
	Vector<InputEventData> eventsTmp; eventsTmp.reserve(_activeEvents.size());
	for (auto &it : _activeEvents) {
		eventsTmp.emplace_back(it.second.first.data);
	}
	return eventsTmp;
}

void InputDispatcher::setListenerExclusive(const InputListener *l) {
	for (auto &it : _activeEvents) {
		setListenerExclusive(it.second.first, it.second.second, l);
	}
	for (auto &it : _activeKeys) {
		setListenerExclusive(it.second.first, it.second.second, l);
	}
}

void InputDispatcher::setListenerExclusiveForTouch(const InputListener *l, uint32_t id) {
	auto it = _activeEvents.find(id);
	if (it != _activeEvents.end()) {
		setListenerExclusive(it->second.first, it->second.second, l);
	}
}

void InputDispatcher::setListenerExclusiveForKey(const InputListener *l, InputKeyCode id) {
	auto it = _activeKeys.find(id);
	if (it != _activeKeys.end()) {
		setListenerExclusive(it->second.first, it->second.second, l);
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

void InputDispatcher::setListenerExclusive(const InputEvent &currentEvent,
		Vector<Rc<InputListener>> &vec, const InputListener *l) const {
	auto v = std::find(vec.begin(), vec.end(), l);
	if (v != vec.end()) {
		if (vec.size() > 1) {
			auto event = currentEvent;
			event.data.event = InputEventName::Cancel;
			for (auto &iit : vec) {
				if (iit != l) {
					iit->handleEvent(event);
				}
			}

			auto tmp = *v;
			vec.clear();
			vec.emplace_back(move(tmp));
		}
	}
}

}
