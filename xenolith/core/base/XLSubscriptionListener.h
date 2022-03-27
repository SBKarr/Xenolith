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


#ifndef XENOLITH_CORE_BASE_XLSUBSCRIPTIONLISTENER_H_
#define XENOLITH_CORE_BASE_XLSUBSCRIPTIONLISTENER_H_

#include "XLDefine.h"
#include "XLScheduler.h"

namespace stappler::xenolith {

template <class T = data::Subscription>
class SubscriptionListener {
public:
	using ListenerCallback = Function<void(data::Subscription::Flags)>;

	SubscriptionListener(const ListenerCallback &cb = nullptr, T *sub = nullptr);
	~SubscriptionListener();

	SubscriptionListener(const SubscriptionListener<T> &);
	SubscriptionListener &operator= (const SubscriptionListener<T> &);

	SubscriptionListener(SubscriptionListener<T> &&);
	SubscriptionListener &operator= (SubscriptionListener<T> &&);

	SubscriptionListener &operator= (T *);

	inline operator T * () { return get(); }
	inline operator T * () const { return get(); }
	inline operator bool () const { return _binding; }

	inline T * operator->() { return get(); }
	inline const T * operator->() const { return get(); }

	void set(T *sub);
	T *get() const;

	void setCallback(const ListenerCallback &cb);
	const ListenerCallback &getCallback() const;

	void setDirty();
	void update(UpdateTime);

	void check();

	void schedule(Scheduler *);
	void unschedule();

protected:
	void updateScheduler();

	data::Binding<T> _binding;
	ListenerCallback _callback;
	bool _dirty = false;
	bool _scheduled = false;
	Scheduler *_scheduler = nullptr;
};


template <class T>
SubscriptionListener<T>::SubscriptionListener(const ListenerCallback &cb, T *sub) : _binding(sub), _callback(cb) {
	static_assert(std::is_convertible<T *, data::Subscription *>::value, "Invalid Type for DataListener<T>!");
	updateScheduler();
}

template <class T>
SubscriptionListener<T>::~SubscriptionListener() {
	unschedule();
}

template <class T>
SubscriptionListener<T>::SubscriptionListener(const SubscriptionListener<T> &other)
: _binding(other._binding), _callback(other._callback) {
	updateScheduler();
}

template <class T>
SubscriptionListener<T> &SubscriptionListener<T>::operator= (const SubscriptionListener<T> &other) {
	_binding = other._binding;
	_callback = other._callback;
	updateScheduler();
	return *this;
}

template <class T>
SubscriptionListener<T>::SubscriptionListener(SubscriptionListener<T> &&other)
: _binding(std::move(other._binding)), _callback(std::move(other._callback)) {
	other.updateScheduler();
	updateScheduler();
}

template <class T>
SubscriptionListener<T> &SubscriptionListener<T>::operator= (SubscriptionListener<T> &&other) {
	_binding = std::move(other._binding);
	_callback = std::move(other._callback);
	other.updateScheduler();
	updateScheduler();
	return *this;
}

template <class T>
void SubscriptionListener<T>::set(T *sub) {
	if (_binding != sub) {
		_binding = Binding<T>(sub);
		updateScheduler();
	}
}

template <class T>
SubscriptionListener<T> &SubscriptionListener<T>::operator= (T *sub) {
	set(sub);
	return *this;
}

template <class T>
T *SubscriptionListener<T>::get() const {
	return _binding;
}

template <class T>
void SubscriptionListener<T>::setCallback(const ListenerCallback &cb) {
	_callback = cb;
}

template <class T>
auto SubscriptionListener<T>::getCallback() const -> const ListenerCallback & {
	return _callback;
}

template <class T>
void SubscriptionListener<T>::setDirty() {
	_dirty = true;
}

template <class T>
void SubscriptionListener<T>::update(UpdateTime dt) {
	if (_callback && _binding) {
		auto val = _binding.check();
		if (!val.empty() || _dirty) {
			_dirty = false;
			_callback(val);
		}
	}
}

template <class T>
void SubscriptionListener<T>::check() {
	update(UpdateTime());
}


template <class T>
void SubscriptionListener<T>::updateScheduler() {
	if (_scheduler) {
		if (_binding && !_scheduled) {
			schedule(_scheduler);
		} else if (!_binding && _scheduled) {
			unschedule();
		}
	}
}

template <class T>
void SubscriptionListener<T>::schedule(Scheduler *sc) {
	_scheduler = sc;
	if (_binding && !_scheduled) {
		sc->scheduleUpdate(this, 0, false);
		_scheduled = true;
	}
}

template <class T>
void SubscriptionListener<T>::unschedule() {
	if (_scheduled && _scheduler) {
		_scheduler->unschedule(this);
		_scheduled = false;
	}
	_scheduler = nullptr;
}

}

#endif /* XENOLITH_CORE_BASE_XLSUBSCRIPTIONLISTENER_H_ */
