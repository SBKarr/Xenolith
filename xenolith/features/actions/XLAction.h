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

#ifndef XENOLITH_FEATURES_ACTIONS_XLACTION_H_
#define XENOLITH_FEATURES_ACTIONS_XLACTION_H_

#include "XLDefine.h"

namespace stappler::xenolith {

class Action : public Ref {
public:
    /** Default tag used for all the actions. */
    static const uint32_t INVALID_TAG = std::numeric_limits<uint32_t>::max();

	virtual ~Action();

	Action();

	/** Return true if the action has finished */
	virtual bool isDone() const;

	/**
	 * Called after the action has finished. It will set the 'target' to nil.
	 * IMPORTANT: You should never call "Action::stop()" manually. Instead, use: "target->stopAction(action);".
	 */
	virtual void invalidate();
	virtual void stop();

	/** Called every frame with it's delta time, dt in seconds. DON'T override unless you know what you are doing */
	virtual void step(float dt);

	/**
	 * Called once per frame. time a value between 0 and 1.

	 * For example:
	 * - 0 Means that the action just started.
	 * - 0.5 Means that the action is in the middle.
	 * - 1 Means that the action is over.
	 *
	 * @param time A value between 0 and 1.
	 */
	virtual void update(float time);

	/** overload point - called when action removed with stopAction* function */
	virtual void onStopped();

	Node* getContainer() const { return _container; }
	Node* getTarget() const { return _target; }

	uint32_t getTag() const { return _tag; }
	void setTag(uint32_t tag) { _tag = tag; }

	float getDuration() const { return _duration; }
	virtual void setDuration(float duration) { _duration = duration; }

protected:
	friend class ActionManager;

	/** Called before the action start. It will also set the target */
	virtual void startWithTarget(Node *target);

	void setContainer(Node *container) { _container = container; }
	void setTarget(Node *target) { _target = target; }

	Node *_container = nullptr;
	/**
	 * The "target".
	 * The target will be set with the 'startWithTarget' method.
	 * When the 'stop' method is called, target will be set to nil.
	 * The target is 'assigned', it is not 'retained'.
	 */
	Node *_target = nullptr;
	/** The action tag. An identifier of the action. */
	uint32_t _tag = INVALID_TAG;

	// duration in seconds or NaN
	float _duration = std::numeric_limits<float>::quiet_NaN();
};

/** @class ActionInterval
@brief An interval action is an action that takes place within a certain period of time.
It has an start time, and a finish time. The finish time is the parameter
duration plus the start time.

These ActionInterval actions have some interesting properties, like:
- They can run normally (default)
- They can run reversed with the reverse method
- They can run with the time altered with the Accelerate, AccelDeccel and Speed actions.

For example, you can simulate a Ping Pong effect running the action normally and
then running it again in Reverse mode.

Example:

Action *pingPongAction = Sequence::actions(action, action->reverse(), nullptr);
*/
class ActionInterval : public Action {
public:
	virtual ~ActionInterval();

	/** Returns a new action that performs the exactly the reverse action */
	virtual Rc<ActionInterval> clone() const {
        XL_ASSERT(0, "");
		return nullptr;
	}

	/** Returns a new action that performs the exactly the reverse action */
	virtual Rc<ActionInterval> reverse() const {
		XL_ASSERT(0, "Reverse action is not defined");
		return nullptr;
	}

	bool init(float duration);

	float getElapsed() { return _elapsed; }

	virtual bool isDone(void) const override;
	virtual void step(float dt) override;
	virtual void startWithTarget(Node *target) override;

	virtual void setDuration(float duration) override;

protected:
    float _elapsed = 0.0f;
    bool _firstTick = true;
};

class Speed : public Action {
public:
	virtual ~Speed();

	Speed();

	bool init(ActionInterval *action, float speed);

	float getSpeed(void) const { return _speed; }
	void setSpeed(float speed) { _speed = speed; }

	ActionInterval* getInnerAction() const { return _innerAction; }
	void setInnerAction(ActionInterval *action);

	virtual Rc<Speed> clone() const;
	virtual Rc<Speed> reverse() const;

	virtual void stop() override;

	virtual void step(float dt) override;

	virtual bool isDone() const override;

	virtual void onStopped() override;

protected:
	virtual void startWithTarget(Node *target) override;

	float _speed = 1.0f;
	Rc<ActionInterval> _innerAction;
};


class Sequence : public ActionInterval {
public:

	virtual Sequence* clone() const override;
	virtual Sequence* reverse() const override;

	virtual void startWithTarget(Node *target) override;
	virtual void stop(void) override;
	/**
	 * @param t In seconds.
	 */
	virtual void update(float t) override;
	virtual void onStopped() override;

	/** initializes the action */
	bool init(Action *pActionOne, Action *pActionTwo);

protected:
	Rc<Action> _actions[2];
	float _split = 0.0f;
	int _last;
};


class TintTo: public ActionInterval {
public:
	virtual ~TintTo();

	bool init(float duration, const Color4F &, ColorMask = ColorMask::Color);

	virtual Rc<ActionInterval> clone() const override;
	virtual Rc<ActionInterval> reverse(void) const override;

	virtual void startWithTarget(Node *target) override;
	virtual void update(float time) override;

protected:
	ColorMask _mask = ColorMask::None;
	Color4F _to;
	Color4F _from;
};

}

#endif /* XENOLITH_FEATURES_ACTIONS_XLACTION_H_ */
