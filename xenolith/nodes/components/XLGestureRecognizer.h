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

#ifndef XENOLITH_NODES_COMPONENTS_XLGESTURERECOGNIZER_H_
#define XENOLITH_NODES_COMPONENTS_XLGESTURERECOGNIZER_H_

#include "XLDefine.h"
#include <bitset>

namespace stappler::xenolith {

static constexpr float TapDistanceAllowed = 16.0f;
static constexpr float TapDistanceAllowedMulti = 32.0f;
static constexpr TimeInterval TapIntervalAllowed = TimeInterval::microseconds(350000ULL);

struct GestureScroll {
	Vec2 pos;
	Vec2 amount;

	const Vec2 &location() const;
	void cleanup();
};

struct GestureTap {
	Vec2 pos;
	uint32_t id = maxOf<uint32_t>();
	uint32_t count = 0;
	Time time;
	float density = 1.0f;

	void cleanup() {
		id = maxOf<uint32_t>();
		time.clear();
		count = 0;
	}
};

enum class GestureType {
	Touch = 1 << 0,
	Scroll = 1 << 1,
};

enum class GestureEvent {
	/** Action just started, listener should return true if it want to "capture" it.
	 * Captured actions will be automatically propagated to end-listener
	 * Other listeners branches will not recieve updates on action, that was not captured by them.
	 * Only one listener on every level can capture action. If one of the listeners return 'true',
	 * action will be captured by this listener, no other listener on this level can capture this action.
	 */
	Began,

	/** Action was activated:
	 * on Touch - touch was moved
	 * on Tap - n-th  tap was recognized
	 * on Press - long touch was recognized
	 * on Swipe - touch was moved
	 * on Pinch - any of two touches was moved, scale was changed
	 * on Rotate - any of two touches was moved, rotation angle was changed
	 */
	Activated,
	Moved = Activated,
	OnLongPress = Activated,

	/** Action was successfully ended, no recognition errors was occurred */
	Ended,

	/** Action was not successfully ended, recognizer detects error in action
	 * pattern and failed to continue recognition */
	Cancelled,
};

class GestureRecognizer : public Ref {
public:
	using EventMask = std::bitset<toInt(InputEventName::Max)>;
	using ButtonMask = std::bitset<toInt(InputMouseButton::Max)>;

	virtual ~GestureRecognizer() { }

	virtual bool init();

	virtual bool canHandleEvent(const InputEvent &event) const;
	virtual bool handleInputEvent(const InputEvent &);

	uint32_t getEventCount() const;
	bool hasEvent(const InputEvent &) const;

	GestureEvent getEvent() const;
	EventMask getEventMask() const;

	virtual void update(uint64_t dt);
	virtual Vec2 getLocation() const;
	virtual void cancel();

protected:
	virtual bool addEvent(const InputEvent &);
	virtual bool removeEvent(const InputEvent &, bool success);
	virtual bool renewEvent(const InputEvent &);

	virtual InputEvent *getTouchById(uint32_t id, uint32_t *index);

	Vector<InputEvent> _events;
	size_t _maxEvents = 0;
	GestureEvent _event = GestureEvent::Cancelled;
	EventMask _eventMask;
	ButtonMask _buttonMask;
};

class GestureTouchRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(GestureEvent, const InputEvent &)>;

	virtual ~GestureTouchRecognizer() { }

	virtual bool init(InputCallback &&, ButtonMask &&);

	virtual bool canHandleEvent(const InputEvent &event) const override;

	void removeRecognizedEvent(uint32_t);

protected:
	virtual bool addEvent(const InputEvent &) override;
	virtual bool removeEvent(const InputEvent &, bool successful) override;
	virtual bool renewEvent(const InputEvent &) override;

	InputCallback _callback;
};

class GestureTapRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<void(GestureEvent, const GestureTap &)>;
	using ButtonMask = std::bitset<toInt(InputMouseButton::Max)>;

public:
	virtual ~GestureTapRecognizer() { }

	virtual bool init(InputCallback &&, ButtonMask &&);

	virtual void update(uint64_t dt) override;
	virtual void cancel() override;

protected:
	virtual bool addEvent(const InputEvent &) override;
	virtual bool removeEvent(const InputEvent &, bool successful) override;
	virtual bool renewEvent(const InputEvent &) override;

	virtual void registerTap();

	GestureTap _gesture;
	InputCallback _callback;
};

class GestureScrollRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(GestureEvent, const GestureScroll &)>;

	virtual ~GestureScrollRecognizer() { }

	virtual bool init(InputCallback &&);

	virtual bool handleInputEvent(const InputEvent &) override;

protected:
	GestureScroll _gesture;
	InputCallback _callback;
};

class GestureMoveRecognizer : public GestureRecognizer {
public:
	using InputCallback = Function<bool(GestureEvent, const InputEvent &)>;

	virtual bool init(InputCallback &&);

	virtual bool handleInputEvent(const InputEvent &) override;

protected:
	InputCallback _callback;
};

std::ostream &operator<<(std::ostream &, GestureEvent);

}

#endif /* XENOLITH_NODES_COMPONENTS_XLGESTURERECOGNIZER_H_ */
