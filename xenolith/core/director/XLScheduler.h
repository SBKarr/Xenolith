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

#ifndef XENOLITH_CORE_DIRECTOR_XLSCHEDULER_H_
#define XENOLITH_CORE_DIRECTOR_XLSCHEDULER_H_

#include "XLDefine.h"
#include "XLLinkedList.h"

namespace stappler::xenolith {

using SchedulerFunc = Function<void(const UpdateTime &)>;

struct SchedulerCallback {
	SchedulerFunc callback = nullptr;
    bool paused = false;
    bool removed = false;

    SchedulerCallback() = default;

    SchedulerCallback(SchedulerFunc &&fn, bool paused)
    : callback(move(fn)), paused(paused) { }
};

class Scheduler : public Ref {
public:
	Scheduler();
	virtual ~Scheduler();

	bool init();

	void unschedule(const void *);
	void unscheduleAll();

	template <typename T>
	void scheduleUpdate(T *, int32_t p = 0, bool paused = false);

	void schedulePerFrame(SchedulerFunc &&callback, void *target, int32_t priority, bool paused);

	void update(const UpdateTime &);

	void resume(void *);
	void pause(void *);

protected:
	struct ScheduledTemporary {
		SchedulerFunc callback;
		void *target;
		int32_t priority;
		bool paused;
	};

	bool _locked = false;
	const void *_currentTarget = nullptr;
	SchedulerCallback *_currentNode = nullptr;
	PriorityList<SchedulerCallback> _list;
	Vector<ScheduledTemporary> _tmp;
};

template <typename T, typename Enable = void>
class SchedulerUpdate {
public:
	static void scheduleUpdate(Scheduler *scheduler, T *target, int32_t p, bool paused) {
		scheduler->schedulePerFrame([target] (const UpdateTime &time) {
			target->update(time);
		}, target, p, paused);
	}
};

template<class T>
class SchedulerUpdate<T, typename std::enable_if<std::is_base_of<Ref, T>::value>::type> {
public:
	static void scheduleUpdate(Scheduler *scheduler, T *t, int32_t p, bool paused) {
		auto ref = static_cast<Ref *>(t);

		scheduler->schedulePerFrame([target = Rc<Ref>(ref)] (const UpdateTime &time) {
			((T *)target.get())->update(time);
		}, t, p, paused);
	}
};

template <typename T>
void Scheduler::scheduleUpdate(T *target, int32_t p, bool paused) {
	SchedulerUpdate<T>::scheduleUpdate(this, target, p, paused);
}

}

#endif /* XENOLITH_CORE_DIRECTOR_XLSCHEDULER_H_ */
