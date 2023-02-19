/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLScheduler.h"

namespace stappler::xenolith {

Scheduler::Scheduler(void) {
    // I don't expect to have more than 30 functions to all per frame
    // _functionsToPerform.reserve(30);
}

Scheduler::~Scheduler(void) {
    unscheduleAll();
}

bool Scheduler::init() {
	return true;
}

void Scheduler::unschedule(const void *ptr) {
	if (_currentTarget == ptr) {
		_currentNode->removed = true;
	} else {
		_list.erase(ptr);
	}
}

void Scheduler::unscheduleAll() {
	_list.clear();
}

void Scheduler::schedulePerFrame(SchedulerFunc &&callback, void *target, int32_t priority, bool paused) {
	if (_locked) {
		// prevent to insert new callbacks when update in progress
		_tmp.emplace_back(ScheduledTemporary{move(callback), target, priority, paused});
	} else {
		_list.emplace(target, priority, move(callback), paused);
	}
}

void Scheduler::update(const UpdateTime &time) {
	_locked = true;

	_list.foreach([&] (void *target, int64_t priority, SchedulerCallback &cb) {
		_currentTarget = target;
		_currentNode = &cb;
		if (!cb.paused) {
			cb.callback(time);
		}
		_currentNode = nullptr;
		_currentTarget = nullptr;
		return cb.removed;
	});

	_locked = false;
	for (auto &it : _tmp) {
		_list.emplace(it.target, it.priority, move(it.callback), it.paused);
	}
}

void Scheduler::resume(void *ptr) {
	if (auto v = _list.find(ptr)) {
		v->paused = false;
	}
}

void Scheduler::pause(void *ptr) {
	if (auto v = _list.find(ptr)) {
		v->paused = true;
	}
}

bool Scheduler::empty() const {
	return _list.empty() && _tmp.empty();
}

}
