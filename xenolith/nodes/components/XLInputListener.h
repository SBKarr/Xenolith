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

#ifndef XENOLITH_NODES_COMPONENTS_XLINPUTLISTENER_H_
#define XENOLITH_NODES_COMPONENTS_XLINPUTLISTENER_H_

#include "XLDefine.h"
#include "XLGestureRecognizer.h"
#include <bitset>

namespace stappler::xenolith {

class Node;
class Scene;
class GestureRecognizer;

class InputListener : public Ref {
public:
	using EventMask = std::bitset<toInt(InputEventName::Max)>;
	using ButtonMask = std::bitset<toInt(InputMouseButton::Max)>;

	template<typename T>
	using InputCallback = Function<bool(GestureEvent, const T &)>;

	using DefaultEventFilter = std::function<bool(const InputEvent &)>;
	using EventFilter = Function<bool(const InputEvent &, const DefaultEventFilter &)>;

	static ButtonMask makeButtonMask(std::initializer_list<InputMouseButton> &&);
	static EventMask makeEventMask(std::initializer_list<InputEventName> &&);

	InputListener();
	virtual ~InputListener();

	bool init(int32_t priority = 0);

	void onEnter(Scene *);
	void onExit();

	void update(UpdateTime);

	bool isRunning() const { return _running; }

	void setEnabled(bool b);
	bool isEnabled() const { return _enabled; }

	void setOwner(Node *pOwner);
	Node* getOwner() const { return _owner; }

	void setPriority(int32_t);
	int32_t getPriority() const { return _priority; }

	void setOpacityFilter(float value) { _opacityFilter = value; }
	float getOpacityFilter() const { return _opacityFilter; }

	void setTouchPadding(float value) { _touchPadding = value; }
	float getTouchPadding() const { return _touchPadding; }

	void setExclusive();
	void setExclusive(uint32_t eventId);

	void setSwallowEvents(EventMask &&);
	void setSwallowAllEvents();
	void setSwallowEvent(InputEventName);
	void clearSwallowAllEvents();
	void clearSwallowEvent(InputEventName);

	void setTouchFilter(const EventFilter &);

	bool shouldSwallowEvent(const InputEvent &) const;
	bool canHandleEvent(const InputEvent &event) const;
	bool handleEvent(const InputEvent &event);

	GestureRecognizer *addTouchRecognizer(InputCallback<InputEvent> &&, ButtonMask && = makeButtonMask({InputMouseButton::MouseLeft}));
	GestureRecognizer *addScrollRecognizer(InputCallback<GestureScroll> &&);
	GestureRecognizer *addMoveRecognizer(InputCallback<InputEvent> &&);

	void setPointerEnterCallback(Function<bool(bool)> &&);
	void setBackgroudCallback(Function<bool(bool)> &&);
	void setFocusCallback(Function<bool(bool)> &&);

	void clear();

protected:
	bool shouldProcessEvent(const InputEvent &) const;
	bool _shouldProcessEvent(const InputEvent &) const; // default realization

	void addEventMask(const EventMask &);

	int32_t _priority = 0; // 0 - scene graph
	bool _enabled = true;
	bool _running = false;
	Node *_owner = nullptr;
	EventMask _eventMask;
	EventMask _swallowEvents;

	float _touchPadding = 0.0f;
	float _opacityFilter = 0.0f;

	Scene *_scene = nullptr;

	EventFilter _eventFilter;
	Vector<Rc<GestureRecognizer>> _recognizers;
	Map<InputEventName, Function<bool(bool)>> _callbacks;
};

}

#endif /* XENOLITH_NODES_COMPONENTS_XLINPUTLISTENER_H_ */
