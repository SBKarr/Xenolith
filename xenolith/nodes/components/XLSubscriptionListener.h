/**
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

#ifndef XENOLITH_CORE_XLSUBSCRIPTIONLISTENER_H_
#define XENOLITH_CORE_XLSUBSCRIPTIONLISTENER_H_

#include "XLComponent.h"
#include "SPSubscription.h"

namespace stappler::xenolith {

class SubscriptionListener : public Component {
public:
	using DirtyCallback = Function<void(SubscriptionFlags)>;

	virtual ~SubscriptionListener() { }

	virtual bool init(DirtyCallback &&cb = nullptr);

	virtual void onEnter(Scene *) override;
	virtual void onExit() override;

	virtual void setCallback(DirtyCallback &&cb);
	virtual const DirtyCallback &getCallback() const { return _callback; }

	virtual void setDirty();
	virtual void update(UpdateTime);

	virtual void check();

protected:
	virtual void updateScheduler();
	virtual void schedule();
	virtual void unschedule();

	Scheduler *_scheduler = nullptr;
	Subscription *_subscription = nullptr;
	DirtyCallback _callback;
	bool _dirty = false;
	bool _scheduled = false;
};

template <typename T = Subscription>
class DataListener : public SubscriptionListener {
public:
	virtual ~DataListener() { }

	virtual bool init(DirtyCallback &&cb = nullptr, T *sub = nullptr) {
		if (!SubscriptionListener::init(move(cb))) {
			return false;
		}

		_binding.set(sub);
		_subscription = sub;
		return true;
	}

	virtual void setSubscription(T *sub) {
		if (_binding != sub) {
			_binding = Binding<T>(sub);
			_subscription = sub;
			updateScheduler();
			setDirty();
		}
	}

	virtual T *getSubscription() const { return _binding.get(); }

	virtual void update(UpdateTime dt) override {
		if (_callback && _binding) {
			auto val = _binding.check();
			if (!val.empty() || _dirty) {
				_dirty = false;
				_callback(val);
			}
		}
	}

protected:
	Binding<T> _binding;
};

}

#endif /* XENOLITH_CORE_XLSUBSCRIPTIONLISTENER_H_ */
