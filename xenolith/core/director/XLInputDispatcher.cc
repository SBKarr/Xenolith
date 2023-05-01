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

#include "XLInputDispatcher.h"
#include "XLInputListener.h"

namespace stappler::xenolith {

InputListenerStorage::~InputListenerStorage() {
	clear();
}

InputListenerStorage::InputListenerStorage(PoolRef *p) : PoolRef(p) {
	perform([&] {
		_preSceneEvents = new (_pool) memory::vector<InputListener *>;
		_sceneEvents = new (_pool) memory::vector<InputListener *>;
		_postSceneEvents = new (_pool) memory::vector<InputListener *>;

		_sceneEvents->reserve(256);
	});
}

void InputListenerStorage::clear() {
	for (auto &it : *_preSceneEvents) {
		it->release(0);
	}
	for (auto &it : *_sceneEvents) {
		it->release(0);
	}
	for (auto &it : *_postSceneEvents) {
		it->release(0);
	}

	_preSceneEvents->clear();
	_sceneEvents->clear();
	_postSceneEvents->clear();
}

void InputListenerStorage::reserve(const InputListenerStorage *st) {
	_preSceneEvents->reserve(st->_preSceneEvents->size());
	_sceneEvents->reserve(st->_sceneEvents->size());
	_postSceneEvents->reserve(st->_postSceneEvents->size());
}

void InputListenerStorage::addListener(InputListener *input) {
	input->retain();
	auto p = input->getPriority();
	if (p == 0) {
		_sceneEvents->emplace_back(input);
	} else if (p < 0) {
		auto lb = std::lower_bound(_postSceneEvents->begin(), _postSceneEvents->end(), input,
				[] (InputListener *l, InputListener *r) {
			return l->getPriority() < r->getPriority();
		});

		if (lb == _postSceneEvents->end()) {
			_postSceneEvents->emplace_back(input);
		} else {
			_postSceneEvents->emplace(lb, input);
		}
	} else {
		auto lb = std::lower_bound(_preSceneEvents->begin(), _preSceneEvents->end(), input,
				[] (InputListener *l, InputListener *r) {
			return l->getPriority() < r->getPriority();
		});

		if (lb == _preSceneEvents->end()) {
			_preSceneEvents->emplace_back(input);
		} else {
			_preSceneEvents->emplace(lb, input);
		}
	}
}

bool InputDispatcher::init(PoolRef *pool, gl::View *view) {
	_textInput = Rc<TextInputManager>::create(view);
	_pool = pool;
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
		req = Rc<InputListenerStorage>::alloc(_pool);
	}
	if (_events) {
		req->reserve(_events);
	}
	return req;
}

void InputDispatcher::commitStorage(Rc<InputListenerStorage> &&storage) {
	_tmpEvents = move(_events);
	_events = move(storage);
	if (_tmpEvents) {
		_tmpEvents->clear();
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
			}
			return true;
		});

		v->second.handle(true);
		break;
	}
	case InputEventName::Move: {
		auto v = _activeEvents.find(event.id);
		if (v != _activeEvents.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle(true);
		}
		break;
	}
	case InputEventName::End:
	case InputEventName::Cancel: {
		auto v = _activeEvents.find(event.id);
		if (v != _activeEvents.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle(false);
			v->second.clear(false);
			_activeEvents.erase(v);
		}
		break;
	}
	case InputEventName::MouseMove: {
		_pointerLocation = Vec2(event.x, event.y);

		EventHandlersInfo handlers{getEventInfo(event)};
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(handlers.event)) {
				handlers.listeners.emplace_back(l);
			}
			return true;
		});

		handlers.handle(false);

		for (auto &it : _activeEvents) {
			if ((it.second.event.data.modifiers & InputModifier::Unmanaged) == InputModifier::None) {
				it.second.event.data.x = event.x;
				it.second.event.data.y = event.y;
				it.second.event.data.event = InputEventName::Move;
				it.second.event.data.modifiers = event.modifiers;
				handleInputEvent(it.second.event.data);
			}
		}
		break;
	}
	case InputEventName::Scroll: {
		EventHandlersInfo handlers{getEventInfo(event)};
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(handlers.event)) {
				handlers.listeners.emplace_back(l);
			}
			return true;
		});
		handlers.handle(false);
		break;
	}
	case InputEventName::Background: {
		_inBackground = event.getValue();

		EventHandlersInfo handlers{getEventInfo(event)};
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(handlers.event)) {
				handlers.listeners.emplace_back(l);
			}
			return true;
		});
		handlers.handle(false);
		break;
	}
	case InputEventName::FocusGain: {
		_hasFocus = event.getValue();

		EventHandlersInfo handlers{getEventInfo(event)};
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(handlers.event)) {
				handlers.listeners.emplace_back(l);
			}
			return true;
		});
		handlers.handle(false);
		break;
	}
	case InputEventName::PointerEnter: {
		_pointerInWindow = event.getValue();

		EventHandlersInfo handlers{getEventInfo(event)};
		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(handlers.event)) {
				handlers.listeners.emplace_back(l);
			}
			return true;
		});

		handlers.handle(false);

		if (!handlers.event.data.getValue()) {
			// Mouse left window, cancel active mouse events
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
				clearKey(event);
				return;
			}
		}

		auto v = resetKey(event);

		_events->foreach([&] (InputListener *l) {
			if (l->canHandleEvent(v->event)) {
				v->listeners.emplace_back(l);
			}
			return true;
		});
		v->handle(true);
		break;
	}
	case InputEventName::KeyRepeated:
		if (_textInput->canHandleInputEvent(event)) {
			// forward to text input
			if (_textInput->handleInputEvent(event)) {
				clearKey(event);
				return;
			}
		}

		handleKey(event, false);
		break;

	case InputEventName::KeyReleased:
	case InputEventName::KeyCanceled:
		if (_textInput->canHandleInputEvent(event)) {
			// forward to text input
			if (_textInput->handleInputEvent(event)) {
				clearKey(event);
				return;
			}
		}

		handleKey(event, true);
		break;
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

void InputDispatcher::EventHandlersInfo::handle(bool removeOnFail) {
	processed.clear();
	if (exclusive) {
		processed.emplace_back(exclusive.get());
		exclusive->handleEvent(event);
	} else {
		Vector<Rc<InputListener>> listenerToRemove;
		auto vec = listeners;
		for (auto &it : vec) {
			processed.emplace_back(it.get());
			auto res = it->handleEvent(event);
			if (res && !exclusive && it->shouldSwallowEvent(event)) {
				setExclusive(it);
			}

			if (exclusive) {
				if (std::find(processed.begin(), processed.end(), exclusive.get()) == processed.end()) {
					exclusive->handleEvent(event);
				}
				break;
			}

			if (removeOnFail && !res) {
				listenerToRemove.emplace_back(it);
			}
		}

		for (auto &it : listenerToRemove) {
			auto iit = std::find(listeners.begin(), listeners.end(), it);
			if (iit != listeners.end()) {
				listeners.erase(iit);
			}
		}
	}
	processed.clear();
}

void InputDispatcher::EventHandlersInfo::clear(bool cancel) {
	if (cancel) {
		event.data.event = InputEventName::Cancel;
		handle(false);
	}

	listeners.clear();
	exclusive = nullptr;
}

void InputDispatcher::EventHandlersInfo::setExclusive(const InputListener *l) {
	if (exclusive) {
		return;
	}

	auto v = std::find(listeners.begin(), listeners.end(), l);
	if (v != listeners.end()) {
		exclusive = *v;

		auto event = this->event;
		event.data.event = InputEventName::Cancel;
		for (auto &iit : listeners) {
			if (iit.get() != l) {
				iit->handleEvent(event);
			}
		}
		listeners.clear();
	}
}

void InputDispatcher::setListenerExclusive(EventHandlersInfo &info, const InputListener *l) const {
	info.setExclusive(l);
}

void InputDispatcher::clearKey(const InputEventData &event) {
	if (event.key.keycode == InputKeyCode::Unknown) {
		auto v = _activeKeySyms.find(event.key.keysym);
		if (v != _activeKeySyms.end()) {
			v->second.clear(true);
			_activeKeySyms.erase(v);
		}
	} else {
		auto v = _activeKeys.find(event.key.keycode);
		if (v != _activeKeys.end()) {
			v->second.clear(true);
			_activeKeys.erase(v);
		}
	}
}

InputDispatcher::EventHandlersInfo *InputDispatcher::resetKey(const InputEventData &event) {
	if (event.key.keycode == InputKeyCode::Unknown) {
		auto v = _activeKeySyms.find(event.key.keysym);
		if (v == _activeKeySyms.end()) {
			v = _activeKeySyms.emplace(event.key.keysym, EventHandlersInfo{getEventInfo(event), Vector<Rc<InputListener>>()}).first;
		} else {
			v->second.clear(true);
			v->second.event = getEventInfo(event);
		}
		return &v->second;
	} else {
		auto v = _activeKeys.find(event.key.keycode);
		if (v == _activeKeys.end()) {
			v = _activeKeys.emplace(event.key.keycode, EventHandlersInfo{getEventInfo(event), Vector<Rc<InputListener>>()}).first;
		} else {
			v->second.clear(true);
			v->second.event = getEventInfo(event);
		}
		return &v->second;
	}
}

void InputDispatcher::handleKey(const InputEventData &event, bool clear) {
	if (event.key.keycode == InputKeyCode::Unknown) {
		auto v = _activeKeySyms.find(event.key.keysym);
		if (v != _activeKeySyms.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle(!clear);
			if (clear) {
				v->second.clear(false);
				_activeKeySyms.erase(v);
			}
		}
	} else {
		auto v = _activeKeys.find(event.key.keycode);
		if (v != _activeKeys.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle(!clear);
			if (clear) {
				v->second.clear(false);
				_activeKeys.erase(v);
			}
		}
	}
}

}
