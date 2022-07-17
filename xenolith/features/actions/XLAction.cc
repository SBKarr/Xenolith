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

void Action::startWithTarget(Node *aTarget) {
	_target = aTarget;
}

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

void Action::onStopped() { }

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

Rc<Speed> Speed::clone() const {
	return Rc<Speed>::create(_innerAction->clone(), _speed);
}

Rc<Speed> Speed::reverse() const {
	return Rc<Speed>::create(_innerAction->reverse(), _speed);
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

void Speed::onStopped() {
	if (_innerAction) {
		_innerAction->onStopped();
	}
}

TintTo::~TintTo() { }

bool TintTo::init(float duration, const Color4F &to, ColorMask mask) {
	if (!ActionInterval::init(duration)) {
		return false;
	}

	_to = to;
	_mask = mask;

	return true;
}

Rc<ActionInterval> TintTo::clone() const {
	auto a = Rc<TintTo>::create(_duration, _to, _mask);
	return a;
}

Rc<ActionInterval> TintTo::reverse(void) const {
    XLASSERT(false, "reverse() not supported in TintTo");
    return nullptr;
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

}
