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

#ifndef XENOLITH_FEATURES_NETWORK_XLNETWORKCONTROLLER_H_
#define XENOLITH_FEATURES_NETWORK_XLNETWORKCONTROLLER_H_

#include "XLDefine.h"
#include "SPNetworkHandle.h"
#include "SPThreadTaskQueue.h"

namespace stappler::xenolith::network {

class Handle;

class Controller final : public thread::ThreadHandlerInterface {
public:
	Controller(Application *, StringView);
	virtual ~Controller();

	Application *getApplication() const { return _application; }
	StringView getName() const { return _name; }

	void run(const Rc<Handle> &);

	void setSignKey(Bytes &&value) { _signKey = move(value); }

protected:
	void onUploadProgress(const Rc<Handle> &, int64_t total, int64_t now);
	void onDownloadProgress(const Rc<Handle> &, int64_t total, int64_t now);
	bool onComplete(const Rc<Handle> &);

	virtual void threadInit() override;
	virtual bool worker() override;
	void cancel();

	virtual void *getSharegroup(StringView);

	void sign(Handle &, NetworkHandle::Context &) const;

	Application *_application = nullptr;
	String _name;
	void *_handle = nullptr;
	std::thread _thread;

	Mutex _mutex;
	std::atomic_flag _shouldQuit;
	Map<void *, Pair<Rc<Handle>, NetworkHandle::Context>> _handles;
	Map<String, void *> _sharegroups;
	Vector<Rc<Handle>> _pending;
	Bytes _signKey;
};

}

#endif /* XENOLITH_FEATURES_NETWORK_XLNETWORKCONTROLLER_H_ */
