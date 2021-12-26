/**
Copyright (c) 2020-2021 Roman Katuntsev <sbkarr@stappler.org>

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

#ifndef COMPONENTS_XENOLITH_PLATFORM_COMMON_XLAPPLICATION_H_
#define COMPONENTS_XENOLITH_PLATFORM_COMMON_XLAPPLICATION_H_

#include "XLDefine.h"
#include "XLEventHeader.h"
#include "XLGlView.h"
#include "XLGlInstance.h"
#include "XLGlRenderQueue.h"
#include "XLStorageServer.h"
#include "SPThreadTaskQueue.h"

namespace stappler::xenolith {

class ResourceCache;

class EventLoop : public Ref {
public:
	EventLoop();
	virtual ~EventLoop();

	virtual bool init(Application *app);
	virtual bool run();

	virtual uint64_t getMinFrameTime() const;
	virtual uint64_t getClock();
	virtual void sleep(uint64_t);

	virtual Pair<uint64_t, uint64_t> getDiskSpace() const;

	virtual void pushEvent(AppEvent::Value);
	virtual AppEvent::Value popEvents();

	virtual void addView(gl::View *);
	virtual void removeView(gl::View *);

protected:
	Application *_application = nullptr;
	std::atomic<AppEvent::Value> _events = AppEvent::None;
};

class Application : public Ref {
public:
	static constexpr uint32_t ApplicationThreadId = 1;

	static EventHeader onDeviceToken;
	static EventHeader onNetwork;
	static EventHeader onUrlOpened;
	static EventHeader onError;

	static EventHeader onRemoteNotification;
	static EventHeader onLaunchUrl;

	struct Data {
		String bundleName = "org.stappler.xenolith";
		String applicationName = "Xenolith";
		String applicationVersion = "0.0.1";
		String userLanguage = "ru-ru";

		String launchUrl;

		Size screenSize = Size(1024, 768);
		bool isPhone = false;
		bool isFixed = false;
		float density = 1.0f;
	};

	static Application *getInstance();

	static int parseOptionString(data::Value &ret, const StringView &str, int argc, const char * argv[]);

public:
	Application();
	virtual ~Application();

	// finalize launch process:
	// - start threads
	// - acquire GL interface/instance
	// - start GL loop
	// - load resource cache
	virtual bool onFinishLaunching();

	// initialize components, that use persistent data storage
	virtual bool onBuildStorage(storage::Server::Builder &);

	// start main application loop, returns when loop is terminated and application should be closed
	virtual bool onMainLoop();

	// Process global Out-of-memory event
	virtual void onMemoryWarning();

	// Process main loop scheduled updates
	// - update thread queue
	// - check for network status
	// - ckeck if DeviceIdentifier was acquired
	virtual void update(uint64_t dt);

	// Update thread queue
	virtual void updateQueue();

	// Run application with parsed command line data
	virtual int run(data::Value &&);

	// Open external URL from within application
	virtual bool openURL(const StringView &url);

public: // Threading, Events
	using Callback = Function<void()>;
	using ExecuteCallback = Function<bool(const Task &)>;
	using CompleteCallback = Function<void(const Task &, bool)>;

	/* Checks if current calling thread is director's main thread */
	bool isMainThread() const;

	/* If current thread is main thread: executes function/task
	   If not: adds function/task to main thread queue */
	void performOnMainThread(const Function<void()> &func, Ref *target = nullptr, bool onNextFrame = false);

	/* If current thread is main thread: executes function/task
	   If not: adds function/task to main thread queue */
    void performOnMainThread(Rc<thread::Task> &&task, bool onNextFrame = false);

	/* Performs action in this thread, task will be constructed in place */
	void perform(const ExecuteCallback &, const CompleteCallback & = nullptr, Ref * = nullptr);

	/* Performs task in thread, identified by id */
    void perform(Rc<thread::Task> &&task);

	/* Performs task in thread, identified by id */
    void perform(Rc<thread::Task> &&task, bool performFirst);

    /* Spawn exclusive thread for task */
	void performAsync(Rc<Task> &&task);

    /* Spawn exclusive thread for task */
	void performAsync(const ExecuteCallback &, const CompleteCallback & = nullptr, Ref * = nullptr);

	/* "Single-threaded" mode allow you to perform async tasks on single thread.
	 When "perform" function is called, task and all subsequent callbacks will be
	 executed on current thread. Perform call returns only when task is performed. */
	void setSingleThreaded(bool value);
	bool isSingleThreaded() const;

	uint64_t getNativeThreadId() const;

	void addEventListener(const EventHandlerNode *listener);
	void removeEventListner(const EventHandlerNode *listener);

	void removeAllListeners();

	void dispatchEvent(const Event &ev);

	// sleep in microseconds
	void sleep(uint64_t);
	uint64_t getClock() const;


	/* device information */

	/* device/OS specific user agent, that used by default browser */
	StringView getUserAgent() const { return _userAgent; }

	/* unique device identifier (UDID). Use OS feature or generates UUID */
	StringView getDeviceIdentifier() const { return _deviceIdentifier; }

	/* Device notification token on APNS or GCM */
	StringView getDeviceToken() const { return _deviceToken; }

	/* Uses XL_MAKE_API_VERSION */
	int64_t getApplicationVersionCode();

	/* Device token for APNS/GCM */
	void registerDeviceToken(const uint8_t *data, size_t len);

	/* Device token for APNS/GCM */
	void registerDeviceToken(const String &data);

	/* networking */
	void setNetworkOnline(bool isOnline);
	bool isNetworkOnline();

	/* application actions */
	void goToUrl(const StringView &url, bool external = true);
	void makePhoneCall(const StringView &number);
	void mailTo(const StringView &address);

	Pair<uint64_t, uint64_t> getTotalDiskSpace();
	uint64_t getApplicationDiskSpace();

	/* device local notification */
	void notification(const String &title, const String &text);

	/* launch with url options */
	// set in launch process by AppController/Activity/etc...
	void setLaunchUrl(const StringView &);

	// called, when we should open recieved url with launched application
	// produce onLaunchUrl event, url can be read from event or by getLaunchUrl()
	void processLaunchUrl(const StringView &);

	const Data &getData() const { return _data; }

	const Rc<thread::TaskQueue> &getQueue() const { return _queue; }
	const gl::Instance *getGlInstance() const { return _instance; }
	const Rc<ResourceCache> &getResourceCache() const { return _resourceCache; }
	const Rc<storage::Server> &getStorageServer() const { return _storageServer; }
	const Rc<network::Controller> &getNetworkController() const { return _networkController; }

protected:
	uint64_t _clockStart = 0;
	data::Value _dbParams;
	String _userAgent;
	String _deviceIdentifier;
	String _deviceToken;

	Data _data;

	uint64_t _updateTimer = 0;
	bool _isNetworkOnline = false;

	Rc<EventLoop> _loop;
	Rc<ResourceCache> _resourceCache;
	Rc<thread::TaskQueue> _queue;
	std::thread::id _threadId;
	bool _singleThreaded = false;

	std::unordered_map<EventHeader::EventID, std::unordered_set<const EventHandlerNode *>> _eventListeners;

	Rc<gl::Instance> _instance; // api instance
	Rc<gl::Loop> _glLoop;
	log::CustomLog _appLog;

	Rc<storage::AssetLibrary> _assetLibrary;
	Rc<storage::Server> _storageServer;
	Rc<network::Controller> _networkController;
};

}

#endif /* COMPONENTS_XENOLITH_PLATFORM_COMMON_XLAPPLICATION_H_ */
