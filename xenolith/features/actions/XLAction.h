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
#include "SPRefContainer.h"

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

	Node* getContainer() const { return _container; }
	Node* getTarget() const { return _target; }

	uint32_t getTag() const { return _tag; }
	void setTag(uint32_t tag) { _tag = tag; }

	float getDuration() const { return _duration; }
	virtual void setDuration(float duration) { _duration = duration; }

	/** Called before the action start. It will also set the target */
	virtual void startWithTarget(Node *target);

protected:
	friend class ActionManager;

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

class ActionInstant : public Action {
public:
	virtual ~ActionInstant();

	virtual bool init(bool runOnce = false);
	virtual void step(float dt) override;

protected:
	bool _runOnce = false;
	bool _performed = false;
};

class Show : public ActionInstant {
public:
	virtual ~Show();

	virtual void update(float time) override;
};

class Hide : public ActionInstant {
public:
	virtual ~Hide();

	virtual void update(float time) override;
};

class ToggleVisibility : public ActionInstant {
public:
	virtual ~ToggleVisibility();

	virtual void update(float time) override;
};

class RemoveSelf : public ActionInstant {
public:
	virtual ~RemoveSelf();

	virtual bool init(bool isNeedCleanUp, bool runOnce = false);
	virtual void update(float time) override;

protected:
	bool _isNeedCleanUp;
};

class Place : public ActionInstant {
public:
	virtual ~Place();

	virtual bool init(const Vec2 &pos, bool runOnce = false);
	virtual void update(float time) override;

protected:
	Vec2 _position;
};

class CallFunc : public ActionInstant {
public:
	virtual ~CallFunc();

	virtual bool init(Function<void()> &&func, bool runOnce = false);
	virtual void update(float time) override;

protected:
	Function<void()> _callback;
};

class ActionInterval : public Action {
public:
	virtual ~ActionInterval();

	bool init(float duration);

	float getElapsed() { return _elapsed; }

	virtual bool isDone(void) const override;
	virtual void step(float dt) override;
	virtual void setDuration(float duration) override;
	virtual void startWithTarget(Node *target) override;

protected:
	float _elapsed = 0.0f;
	bool _firstTick = true;
};

/** @class Speed
 * @brief Changes the speed of an action, making it take longer (speed>1)
 * or less (speed<1) time.
 * Useful to simulate 'slow motion' or 'fast forward' effect.
 * @warning This action can't be Sequenceable because it is not an IntervalAction.
 */
class Speed : public Action {
public:
	virtual ~Speed();

	Speed();

	bool init(ActionInterval *action, float speed);

	float getSpeed(void) const { return _speed; }
	void setSpeed(float speed) { _speed = speed; }

	ActionInterval* getInnerAction() const { return _innerAction; }
	void setInnerAction(ActionInterval *action);

	virtual void stop() override;
	virtual void step(float dt) override;
	virtual bool isDone() const override;

	virtual void startWithTarget(Node *target) override;

protected:
	float _speed = 1.0f;
	Rc<ActionInterval> _innerAction;
};

class Sequence : public ActionInterval {
public:
	virtual ~Sequence();

	template <typename ... Args>
	bool init(Args && ... args) {
		_duration = 0.0f;
		reserve(sizeof...(Args));
		return initWithActions(std::forward<Args>(args)...);
	}

	virtual void stop(void) override;
	virtual void update(float t) override;
	virtual void startWithTarget(Node *target) override;

protected:
	bool reserve(size_t);
	bool addAction(Function<void()> &&); // add callback action
	bool addAction(float); // add timeout action

	template <typename T>
	bool addAction(const Rc<T> &t) {
		return addAction(t.get());
	}

	bool addAction(Action *);

	template <typename T, typename ... Args>
	bool initWithActions(T &&t, Args && ... args) {
		if (!addAction(std::forward<T>(t))) {
			return false;
		}
		return initWithActions(std::forward<Args>(args)...);
	}

	template <typename T>
	bool initWithActions(T &&t) {
		if (addAction(std::forward<T>(t))) {
			return ActionInterval::init(_duration);
		}
		return false;
	}

	struct ActionData {
		Rc<Action> action;
		float minThreshold = 0.0f;
		float maxThreshold = 0.0f;
		float threshold = 0.0f;
	};

	Vector<ActionData> _actions;
	float _prevTime = 0.0f;
	uint32_t _currentIdx = 0;
};

class Spawn : public ActionInterval {
public:
	virtual ~Spawn();

	template <typename ... Args>
	bool init(Args && ... args) {
		_duration = 0.0f;
		reserve(sizeof...(Args));
		return initWithActions(std::forward<Args>(args)...);
	}

	virtual void stop(void) override;
	virtual void update(float t) override;
	virtual void startWithTarget(Node *target) override;

protected:
	bool reserve(size_t);
	bool addAction(Function<void()> &&); // add callback action
	bool addAction(float); // add timeout action

	template <typename T>
	bool addAction(const Rc<T> &t) {
		return addAction(t.get());
	}

	bool addAction(Action *);

	template <typename T, typename ... Args>
	bool initWithActions(T &&t, Args && ... args) {
		if (!addAction(std::forward<T>(t))) {
			return false;
		}
		return initWithActions(std::forward<Args>(args)...);
	}

	template <typename T>
	bool initWithActions(T &&t) {
		if (addAction(std::forward<T>(t))) {
			return ActionInterval::init(_duration);
		}
		return false;
	}

	struct ActionData {
		Rc<Action> action;
		float threshold = 0.0f;
	};

	Vector<ActionData> _actions;
	float _prevTime = 0.0f;
	uint32_t _currentIdx = 0;
};

/** @class DelayTime
 * @brief Delays the action a certain amount of seconds.
 */
class DelayTime : public ActionInterval {
public:
	virtual ~DelayTime();

	virtual void update(float time) override;
};

class TintTo : public ActionInterval {
public:
	virtual ~TintTo();

	bool init(float duration, const Color4F &, ColorMask = ColorMask::Color);

	virtual void update(float time) override;

protected:
	virtual void startWithTarget(Node *target) override;

	ColorMask _mask = ColorMask::None;
	Color4F _to;
	Color4F _from;
};

class ActionProgress : public ActionInterval {
public:
	using StartCallback = Function<void()>;
	using UpdateCallback = Function<void(float progress)>;
	using StopCallback = Function<void()>;

    virtual bool init(float duration,
    		UpdateCallback &&update, StartCallback &&start = nullptr, StopCallback &&stop = nullptr);

    virtual bool init(float duration, float targetProgress,
    		UpdateCallback &&update, StartCallback &&start = nullptr, StopCallback &&stop = nullptr);

    virtual bool init(float duration, float sourceProgress, float targetProgress,
    		UpdateCallback &&update, StartCallback &&start = nullptr, StopCallback &&stop = nullptr);

    virtual void startWithTarget(Node *t) override;
    virtual void update(float time) override;
    virtual void stop() override;

    void setSourceProgress(float progress) { _sourceProgress = progress; }
    float getSourceProgress() const { return _sourceProgress; }

    void setTargetProgress(float progress) { _targetProgress = progress; }
    float getTargetProgress() const { return _targetProgress; }

    void setStartCallback(StartCallback &&cb) { _onStart = move(cb); }
    void setUpdateCallback(UpdateCallback &&cb) { _onUpdate = move(cb); }
    void setStopCallback(StopCallback &&cb) { _onStop = move(cb); }

protected:
    bool _stopped = true;
    float _sourceProgress = 0.0f;
    float _targetProgress = 1.0f;

    StartCallback _onStart;
    UpdateCallback _onUpdate;
    StopCallback _onStop;
};

}

#endif /* XENOLITH_FEATURES_ACTIONS_XLACTION_H_ */
