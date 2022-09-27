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

#include "XLAction.h"

namespace stappler::xenolith {

Action::~Action() { }

Action::Action() { }

void Action::invalidate() {
	stop();
}

void Action::stop() {
	_target = nullptr;
}

bool Action::isDone() const {
	return _target == nullptr;
}

void Action::step(float dt) {
	log::text("Action", "[step]: override me");
}

void Action::update(float time) {
	log::text("Action", "[update]: override me");
}

void Action::startWithTarget(Node *aTarget) {
	_target = aTarget;
}

ActionInstant::~ActionInstant() { }

bool ActionInstant::init(bool runOnce) {
	_duration = 0.0f;
	_runOnce = runOnce;
	return true;
}

void ActionInstant::step(float dt) {
	if (!_performed || !_runOnce) {
	    update(1.0f);
	    _performed = true;
	}
}

Show::~Show() { }

void Show::update(float time) {
	_target->setVisible(true);
}

Hide::~Hide() { }

void Hide::update(float time) {
	_target->setVisible(false);
}

ToggleVisibility::~ToggleVisibility() { }

void ToggleVisibility::update(float time) {
	_target->setVisible(!_target->isVisible());
}

RemoveSelf::~RemoveSelf() { }

bool RemoveSelf::init(bool isNeedCleanUp, bool runOnce) {
	if (!ActionInstant::init(runOnce)) {
		return false;
	}

	_isNeedCleanUp = isNeedCleanUp;
	return true;
}

void RemoveSelf::update(float time) {
	_target->removeFromParent(_isNeedCleanUp);
}

Place::~Place() { }

bool Place::init(const Vec2 &pos, bool runOnce) {
	if (!ActionInstant::init(runOnce)) {
		return false;
	}

	_position = pos;
	return true;
}

void Place::update(float time) {
	_target->setPosition(_position);
}

CallFunc::~CallFunc() { }

bool CallFunc::init(Function<void()> &&func, bool runOnce) {
	if (!ActionInstant::init(runOnce)) {
		return false;
	}

	_callback = move(func);
	return true;
}

void CallFunc::update(float time) {
	_callback();
}

ActionInterval::~ActionInterval() { }

bool ActionInterval::init(float duration) {
	_duration = duration;

	// prevent division by 0
	// This comparison could be in step:, but it might decrease the performance
	// by 3% in heavy based action games.
	_duration = std::max(_duration, FLT_EPSILON);
	if (_duration == 0) {
		_duration = FLT_EPSILON;
	}

	_elapsed = 0;
	_firstTick = true;
	return true;
}

bool ActionInterval::isDone() const {
	return _elapsed >= _duration;
}

void ActionInterval::step(float dt) {
	if (_firstTick) {
		_firstTick = false;
		_elapsed = 0;
	} else {
		_elapsed += dt;
	}

	this->update(math::clamp(_elapsed / _duration, 0.0f, 1.0f));
}

void ActionInterval::startWithTarget(Node *target) {
	Action::startWithTarget(target);
	_elapsed = 0.0f;
	_firstTick = true;
}

void ActionInterval::setDuration(float duration) {
	_duration = std::max(_duration, FLT_EPSILON);
}

Speed::~Speed() { }

Speed::Speed() { }

bool Speed::init(ActionInterval *action, float speed) {
	XLASSERT(action != nullptr, "action must not be NULL");
	_innerAction = action;
	_speed = speed;
	return true;
}

void Speed::setInnerAction(ActionInterval *action) {
	if (_innerAction != action) {
		_innerAction = action;
	}
}

void Speed::startWithTarget(Node *target) {
	Action::startWithTarget(target);
	_innerAction->startWithTarget(target);
}

void Speed::stop() {
	_innerAction->stop();
	Action::stop();
}

void Speed::step(float dt) {
	_innerAction->step(dt * _speed);
}

bool Speed::isDone() const {
	return _innerAction->isDone();
}

Sequence::~Sequence() { }

void Sequence::stop(void) {
	ActionInterval::stop();
	if (_prevTime < 1.0f) {
		auto front = _actions.begin() + _currentIdx;
		auto end = _actions.end();

		front->action->stop();
		++ front;
		++ _currentIdx;

		while (front != end && front->threshold <= std::numeric_limits<float>::epsilon()) {
			front->action->startWithTarget(_target);
			front->action->update(1.0);
			front->action->stop();

			// std::cout << "Index: " << _currentIdx << "\n";

			++ front;
			++ _currentIdx;
		}

		// do not update any non-instant actions, just start-stop
		while (front != end) {
			front->action->startWithTarget(_target);
			front->action->stop();

			++ front;
			++ _currentIdx;
		}

		_prevTime = 1.0f;
	}
}

void Sequence::update(float t) {
	auto front = _actions.begin() + _currentIdx;
	auto end = _actions.end();

	auto dt = t - _prevTime;

	// assume monotonical progress

	while (front != end && dt != 0) {
		// process instants
		if (front->threshold <= std::numeric_limits<float>::epsilon()) {
			do {
				front->action->startWithTarget(_target);
				front->action->update(1.0);
				front->action->stop();

				// std::cout << "Index: (instant) " << _currentIdx << "\n";

				++ front;
				++ _currentIdx;
			} while (front != end && front->threshold == 0.0f);

			// start next non-instant
			if (front == end) {
				_prevTime = t;
				return;
			} else {
				front->action->startWithTarget(_target);
				front->action->update(0.0);
			}
		}

		auto timeFromActionStart = t - front->minThreshold;
		auto actionRelativeTime = timeFromActionStart / front->threshold;

		if (actionRelativeTime >= 1.0f - std::numeric_limits<float>::epsilon()) {
			front->action->update(1.0f);
			dt = t - front->maxThreshold;
			front->action->stop();

			// std::cout << "Index: (interval) " << _currentIdx << "\n";

			++ front;
			++ _currentIdx;

			// start next non-instant
			if (front == end) {
				_prevTime = t;
				return;
			} else if (front->threshold > std::numeric_limits<float>::epsilon()) {
				front->action->startWithTarget(_target);
				front->action->update(0.0);
			}
		} else {
			front->action->update(actionRelativeTime);
			dt = 0.0f;
			break;
		}
	}

	auto tmp = front;
	while (front != end && front->threshold <= std::numeric_limits<float>::epsilon()) {
		front->action->startWithTarget(_target);
		front->action->update(1.0);
		front->action->stop();

		// std::cout << "Index: " << _currentIdx << "\n";

		++ front;
		++ _currentIdx;
	}

	if (front != end && tmp != front) {
		front->action->startWithTarget(_target);
		front->action->update(0.0);
	}

	_prevTime = t;
}

void Sequence::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	float threshold = 0.0f;
	for (auto &it : _actions) {
		it.minThreshold = threshold;
		it.threshold = it.action->getDuration() / _duration;
		threshold += it.threshold;
		it.maxThreshold = threshold;
	}

	// start first action if it's not instant
	if (_actions.front().threshold != 0.0f) {
		_actions.front().action->startWithTarget(_target);
	}
}

bool Sequence::reserve(size_t s) {
	_actions.reserve(s);
	return true;
}

bool Sequence::addAction(Function<void()> &&cb) {
	auto a = Rc<CallFunc>::create(move(cb));
	return addAction(a.get());
}

bool Sequence::addAction(float time) {
	auto a = Rc<DelayTime>::create(time);
	return addAction(a.get());
}

bool Sequence::addAction(Action *a) {
	_duration += a->getDuration();
	_actions.emplace_back(ActionData{a});
	return true;
}

Spawn::~Spawn() { }

void Spawn::stop(void) {
	ActionInterval::stop();
	if (_prevTime < 1.0f) {
		for (auto &it : _actions) {
			if (it.threshold >= _prevTime) {
				it.action->stop();
			}
		}
		_prevTime = 1.0f;
	}
}

void Spawn::update(float t) {
	for (auto &it : _actions) {
		if (t >= it.threshold && _prevTime < it.threshold) {
			it.action->update(1.0f);
			it.action->stop();
		} else if (t < it.threshold) {
			it.action->update( t * it.threshold );
		}
	}

	_prevTime = t;
}

void Spawn::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	for (auto &it : _actions) {
		it.threshold = it.action->getDuration() / _duration - std::numeric_limits<float>::epsilon();
		it.action->startWithTarget(target);
	}

	_prevTime = - std::numeric_limits<float>::epsilon() * 2;
}

bool Spawn::reserve(size_t s) {
	_actions.reserve(s);
	return true;
}

bool Spawn::addAction(Function<void()> &&cb) {
	auto a = Rc<CallFunc>::create(move(cb));
	return addAction(a.get());
}

bool Spawn::addAction(float time) {
	auto a = Rc<DelayTime>::create(time);
	return addAction(a.get());
}

bool Spawn::addAction(Action *a) {
	_duration = std::max(_duration, a->getDuration());
	_actions.emplace_back(ActionData{a});
	return true;
}

DelayTime::~DelayTime() { }

void DelayTime::update(float time) { }

TintTo::~TintTo() { }

bool TintTo::init(float duration, const Color4F &to, ColorMask mask) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_to = to;
	_mask = mask;

	return true;
}

void TintTo::startWithTarget(Node *target) {
	ActionInterval::startWithTarget(target);
	if (target) {
		_from = target->getColor();
		_to.setUnmasked(_from, _mask);
	}
}

void TintTo::update(float time) {
	_target->setColor(progress(_from, _to, time), true);
}

bool ActionProgress::init(float duration, UpdateCallback &&update, StartCallback &&start, StopCallback &&stop) {
	return init(duration, 0.0f, 1.0f, move(update), move(start), move(stop));
}

bool ActionProgress::init(float duration, float targetProgress, UpdateCallback &&update, StartCallback &&start, StopCallback &&stop) {
	return init(duration, 0.0f, targetProgress, move(update), move(start), move(stop));
}

bool ActionProgress::init(float duration, float sourceProgress, float targetProgress,
		UpdateCallback &&update, StartCallback &&start, StopCallback &&stop) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_sourceProgress = sourceProgress;
	_targetProgress = targetProgress;
	_onUpdate = move(update);
	_onStart = move(start);
	_onStop = move(stop);

	return true;
}

void ActionProgress::startWithTarget(Node *t) {
	ActionInterval::startWithTarget(t);
	_stopped = false;
	if (_onStart) {
		_onStart();
	}
}

void ActionProgress::update(float time) {
	if (_onUpdate) {
		_onUpdate(_sourceProgress + (_targetProgress - _sourceProgress) * time);
	}
}

void ActionProgress::stop() {
	if (!_stopped && _onStop) {
		_onStop();
	}
	_stopped = true;
	ActionInterval::stop();
}

}
