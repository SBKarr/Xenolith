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

#ifndef XENOLITH_CORE_DIRECTOR_XLINPUTDISPATCHER_H_
#define XENOLITH_CORE_DIRECTOR_XLINPUTDISPATCHER_H_

#include "XLInputListener.h"
#include "XLTextInputManager.h"

namespace stappler::xenolith {

class InputListenerStorage : public PoolRef {
public:
	virtual ~InputListenerStorage();

	InputListenerStorage(PoolRef *);

	void clear();
	void reserve(const InputListenerStorage *);

	void addListener(InputListener *);

	template <typename Callback>
	bool foreach(const Callback &);

protected:
	memory::vector<InputListener *> *_preSceneEvents;
	memory::vector<InputListener *> *_sceneEvents; // in reverse order
	memory::vector<InputListener *> *_postSceneEvents;
};

class InputDispatcher : public Ref {
public:
	virtual ~InputDispatcher() { }

	bool init(PoolRef *, gl::View *);

	void update(const UpdateTime &time);

	Rc<InputListenerStorage> acquireNewStorage();
	void commitStorage(Rc<InputListenerStorage> &&);

	void handleInputEvent(const InputEventData &);

	Vector<InputEventData> getActiveEvents() const;

	void setListenerExclusive(const InputListener *l);
	void setListenerExclusiveForTouch(const InputListener *l, uint32_t);
	void setListenerExclusiveForKey(const InputListener *l, InputKeyCode);

	const Rc<TextInputManager> &getTextInputManager() const { return _textInput; }

	bool isInBackground() const { return _inBackground; }
	bool hasFocus() const { return _hasFocus; }
	bool isPointerWithinWindow() const { return _pointerInWindow; }

protected:
	InputEvent getEventInfo(const InputEventData &) const;
	void updateEventInfo(InputEvent &, const InputEventData &) const;

	struct EventHandlersInfo {
		InputEvent event;
		Vector<Rc<InputListener>> listeners;
		Rc<InputListener> exclusive;
		InputListener *current = nullptr;
		bool exclusiveDirty = true;

		void handle(bool removeOnFail);
		void clear(bool cancel);
	};

	void setListenerExclusive(EventHandlersInfo &, const InputListener *l) const;

	void clearKey(const InputEventData &);
	EventHandlersInfo *resetKey(const InputEventData &);
	void handleKey(const InputEventData &, bool clear);

	uint64_t _currentTime = 0;
	HashMap<uint32_t, EventHandlersInfo> _activeEvents;
	HashMap<InputKeyCode, EventHandlersInfo> _activeKeys;
	HashMap<uint32_t, EventHandlersInfo> _activeKeySyms;
	Rc<InputListenerStorage> _events;
	Rc<InputListenerStorage> _tmpEvents;
	Rc<TextInputManager> _textInput;
	Rc<PoolRef> _pool;

	Vec2 _pointerLocation = Vec2::ZERO;
	bool _inBackground = false;
	bool _hasFocus = true;
	bool _pointerInWindow = false;
};

template <typename Callback>
bool InputListenerStorage::foreach(const Callback &cb) {
	memory::vector<InputListener *>::reverse_iterator it, end;
	it = _preSceneEvents->rbegin();
	end = _preSceneEvents->rend();

	for (;it != end; ++ it) {
		if (!cb(*it)) {
			return false;
		}
	}

	it = _sceneEvents->rbegin();
	end = _sceneEvents->rend();

	for (;it != end; ++ it) {
		if (!cb(*it)) {
			return false;
		}
	}

	it = _postSceneEvents->rbegin();
	end = _postSceneEvents->rend();

	for (;it != end; ++ it) {
		if (!cb(*it)) {
			return false;
		}
	}

	return true;
}

}

#endif /* XENOLITH_CORE_DIRECTOR_XLINPUTDISPATCHER_H_ */
