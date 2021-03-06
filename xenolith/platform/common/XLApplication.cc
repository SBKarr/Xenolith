/**
Copyright (c) 2020-2022 Roman Katuntsev <sbkarr@stappler.org>

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

#include "XLApplication.h"
#include "XLPlatform.h"
#include "XLDirector.h"
#include "XLEvent.h"
#include "XLEventHandler.h"
#include "XLGlInstance.h"
#include "XLGlLoop.h"

namespace stappler::log {

void __xenolith_log(const StringView &tag, CustomLog::Type t, CustomLog::VA &va);

}

namespace stappler::xenolith {

XL_DECLARE_EVENT_CLASS(Application, onDeviceToken);
XL_DECLARE_EVENT_CLASS(Application, onNetwork);
XL_DECLARE_EVENT_CLASS(Application, onUrlOpened);
XL_DECLARE_EVENT_CLASS(Application, onError);

static Application * s_application = nullptr;

Application *Application::getInstance() {
	return s_application;
}

int Application::parseOptionString(Value &ret, const StringView &str, int argc, const char * argv[]) {
	if (str.starts_with("w=") == 0) {
		auto s = str.sub(2).readInteger().get(0);
		if (s > 0) {
			ret.setInteger(s, "width");
		}
	} else if (str.starts_with("h=") == 0) {
		auto s = str.sub(2).readInteger().get(0);
		if (s > 0) {
			ret.setInteger(s, "height");
		}
	} else if (str.starts_with("d=") == 0) {
		auto s = str.sub(2).readDouble().get(0.0);
		if (s > 0) {
			ret.setDouble(s, "density");
		}
	} else if (str.starts_with("l=") == 0) {
		ret.setString(str.sub(2), "locale");
	} else if (str == "phone") {
		ret.setBool(true, "phone");
	} else if (str == "package") {
		ret.setString(argv[0], "package");
	} else if (str == "fixed") {
		ret.setBool(true, "fixed");
	} else if (str == "renderdoc") {
		ret.setBool(true, "renderdoc");
	}
	return 1;
}

Application::Application() : _appLog(&log::__xenolith_log) {
	XLASSERT(s_application == nullptr, "Application should be only one");

	memory::pool::initialize();

	_rootPool = memory::pool::create(memory::pool::acquire());
	_updatePool = memory::pool::create(_rootPool);

	_clockStart = platform::device::_clock(platform::device::Monotonic);

	_userAgent = platform::device::_userAgent();
	_deviceIdentifier  = platform::device::_deviceIdentifier();
	_isNetworkOnline = platform::network::_isNetworkOnline();

	platform::network::_setNetworkCallback([this] (bool online) {
		if (online != _isNetworkOnline) {
			setNetworkOnline(online);
		}
	});

	s_application = this;

#if MODULE_XNOLITH_STORAGE
	db::setStorageRoot(&_storageRoot);

	_networkController = Rc<network::Controller>::alloc(this, "Root");

	auto libpath = filesystem::writablePath<Interface>("library");
	filesystem::mkdir(libpath);

	_assetLibrary = Rc<storage::AssetLibrary>::create(this, Value({
		pair("driver", data::Value("sqlite")),
		pair("dbname", data::Value(toString(libpath, "/assets.v2.db"))),
		pair("serverName", data::Value("AssetStorage"))
	}));
#endif
}

Application::~Application() {
	memory::pool::destroy(_updatePool);
	memory::pool::destroy(_rootPool);
	memory::pool::terminate();
}

bool Application::onFinishLaunching() {
	_threadId = std::this_thread::get_id();

	thread::ThreadInfo::setMainThread();

	_queue = Rc<thread::TaskQueue>::alloc("Main");

	if (!_queue->spawnWorkers(thread::TaskQueue::Flags::Waitable, ApplicationThreadId,
			config::getMainThreadCount(), _queue->getName())) {
		log::text("Application", "Fail to spawn worker threads");
		return false;
	}

	_instance = platform::graphic::createInstance(this);

	if (!_instance) {
		_instance = nullptr;
		log::text("Application", "Fail to create graphic api instance");
		return false;
	}

	if (_instance->getAvailableDevices().empty()) {
		_instance = nullptr;
		log::text("Application", "No devices found");
		return false;
	}

	_glLoop = _instance->makeLoop(this, gl::Instance::DefaultDevice);

	return _glLoop != nullptr;
}

#if MODULE_XENOLITH_STORAGE
bool Application::onBuildStorage(storage::Server::Builder &builder) {
	return true;
}
#endif

bool Application::onMainLoop() {
	return false;
}

void Application::onMemoryWarning() {

}

int Application::run(Value &&data) {
	memory::pool::push(_updatePool);

#if MODULE_XENOLITH_STORAGE
	_dbParams = data::Value({
		pair("driver", data::Value("sqlite")),
		pair("dbname", data::Value(filesystem::cachesPath("root.sqlite"))),
		pair("serverName", data::Value("RootStorage"))
	});
#endif

	for (auto &it : data.asDict()) {
		if (it.first == "width") {
			if (it.second.isInteger()) {
				_data.screenSize.width = float(it.second.getInteger());
			} else if (it.second.isDouble()) {
				_data.screenSize.width = float(it.second.getDouble());
			}
		} else if (it.first == "height") {
			if (it.second.isInteger()) {
				_data.screenSize.height = float(it.second.getInteger());
			} else if (it.second.isDouble()) {
				_data.screenSize.height = float(it.second.getDouble());
			}
		} else if (it.first == "density") {
			if (it.second.isInteger()) {
				_data.density = float(it.second.getInteger());
			} else if (it.second.isDouble()) {
				_data.density = float(it.second.getDouble());
			}
		} else if (it.first == "locale") {
			if (it.second.isString() && !it.second.getString().empty()) {
				_data.userLanguage = it.second.getString();
			}
		} else if (it.first == "bundle") {
			if (it.second.isString() && !it.second.getString().empty()) {
				_data.bundleName = it.second.getString();
			}
		} else if (it.first == "phone") {
			_data.isPhone = it.second.getBool();
		} else if (it.first == "fixed") {
			_data.isFixed = it.second.getBool();
		} else if (it.first == "renderdoc") {
			_data.renderdoc = true;
		}
	}

	if (!onFinishLaunching()) {
		log::text("Application", "Fail to launch application: onFinishLaunching failed");
		memory::pool::pop();
		return 1;
	}

	// all init parallel with gl-loop init goes here

#if MODULE_XENOLITH_STORAGE
	_storageServer = Rc<storage::Server>::create(this, _dbParams, [&] (storage::Server::Builder &builder) {
		return onBuildStorage(builder);
	});

	if (!_storageServer) {
		log::text("Application", "Fail to launch application: onBuildStorage failed");
		memory::pool::pop();
		return 1;
	}
#endif

	bool ret = false;
	if (_glLoop) {
		// ensure gl thread initialized
		_glLoop->waitRinning();

		_fontLibrary = Rc<font::FontLibrary>::create(_glLoop);

		auto builder = _fontLibrary->makeDefaultControllerBuilder("ApplicationFontController");

		updateDefaultFontController(builder);

		_fontController = _fontLibrary->acquireController(move(builder));

		ret = onMainLoop();

		_fontController = nullptr;
		_fontLibrary = nullptr;

		_glLoop->cancel();
		_glLoop = nullptr;
	} else {
		log::text("Application", "Fail to launch gl loop: onFinishLaunching failed");
	}

	if (_queue) {
		_queue->cancelWorkers();
		_queue = nullptr;
	}

	if (_instance) {
		if (_instance->getReferenceCount() > 1) {
			auto instance = _instance.get();
			_instance = nullptr;

			log::vtext("gl::Instance", "Backtrace for ", (void *)instance);
			instance->foreachBacktrace([] (uint64_t id, Time time, const std::vector<std::string> &vec) {
				StringStream stream;
				stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
				for (auto &it : vec) {
					stream << "\t" << it << "\n";
				}
				log::text("gl::Instance", stream.str());
			});
		}
		_instance = nullptr;
	}

	memory::pool::pop();
    return ret ? 0 : -1;
}

bool Application::openURL(const StringView &url) {
	return platform::interaction::_goToUrl(url, true);
}

void Application::addView(gl::ViewInfo &&view) {
	_glLoop->addView(move(view));
}

void Application::runLoop(TimeInterval iv) {
	uint32_t count = 0;
	uint64_t clock = platform::device::_clock(platform::device::ClockType::Monotonic);
	uint64_t lastUpdate = clock;
	do {
		count = 0;
		if (!_immediateUpdate) {
			_queue->wait(iv - TimeInterval::microseconds(clock - lastUpdate), &count);
		}
		if (count > 0) {
			memory::pool::push(_updatePool);
			_queue->update();
			memory::pool::pop();
			memory::pool::clear(_updatePool);
		}
		clock = platform::device::_clock(platform::device::ClockType::Monotonic);

		auto dt = TimeInterval::microseconds(clock - lastUpdate);
		if (dt >= iv || _immediateUpdate) {
			update(dt.toMicros());
			lastUpdate = clock;
		}
	} while (!_shouldEndLoop);
}

void Application::end() {
	performOnMainThread([&] {
		_shouldEndLoop = true;
	}, this);
}

void Application::update(uint64_t dt) {
	memory::pool::push(_updatePool);

	if (!_isNetworkOnline) {
		_updateTimer += dt;
		if (_updateTimer >= 10'000'000) {
			_updateTimer = -10'000'000;
			setNetworkOnline(platform::network::_isNetworkOnline());
		}
	}

	if (_deviceIdentifier.empty()) {
		_deviceIdentifier = platform::device::_deviceIdentifier();
	}

	if (_fontController) {
		_fontController->update();
	}

	if (_fontLibrary) {
		_fontLibrary->update();
	}

	memory::pool::pop();
	memory::pool::clear(_updatePool);
}

void Application::registerDeviceToken(const uint8_t *data, size_t len) {
    registerDeviceToken(base16::encode<Interface>(CoderSource(data, len)));
}

void Application::registerDeviceToken(const String &data) {
	_deviceToken = data;
	if (!_deviceToken.empty()) {
		onDeviceToken(this, _deviceToken);
	}
}

void Application::setNetworkOnline(bool online) {
	if (_isNetworkOnline != online) {
		_isNetworkOnline = online;
		onNetwork(this, _isNetworkOnline);
		if (!_isNetworkOnline) {
			_updateTimer = 0;
		}
	}
}

bool Application::isNetworkOnline() {
	return _isNetworkOnline;
}

void Application::goToUrl(const StringView &url, bool external) {
	onUrlOpened(this, url);
	platform::interaction::_goToUrl(url, external);
}
void Application::makePhoneCall(const StringView &number) {
	onUrlOpened(this, number);
	platform::interaction::_makePhoneCall(number);
}
void Application::mailTo(const StringView &address) {
	onUrlOpened(this, address);
	platform::interaction::_mailTo(address);
}

uint64_t Application::getApplicationDiskSpace() {
	auto path = filesystem::writablePath<Interface>(_data.bundleName);
	uint64_t size = 0;
	filesystem::ftw(path, [&size] (const StringView &path, bool isFile) {
		if (isFile) {
			filesystem::Stat stat;
			if (filesystem::stat(path, stat)) {
				size += stat.size;
			}
		}
	});

	path = filesystem::cachesPath<Interface>(_data.bundleName);
	filesystem::ftw(path, [&size] (const StringView &path, bool isFile) {
		if (isFile) {
			filesystem::Stat stat;
			if (filesystem::stat(path, stat)) {
				size += stat.size;
			}
		}
	});

	return size;
}

int64_t Application::getApplicationVersionCode() {
	static int64_t version = 0;
	if (version == 0) {
		String str(_data.applicationVersion);
		int major = 0, middle = 0, minor = 0, state = 0;
		for (char c : str) {
			if (c == '.') {
				state++;
			} else if (c >= '0' && c <= '9') {
				if (state == 0) {
					major = major * 10 + (c - '0');
				} else if (state == 1) {
					middle = middle * 10 + (c - '0');
				} else if (state == 2) {
					minor = minor * 10 + (c - '0');
				} else {
					break;
				}
			} else {
				break;
			}
		}
		version = XL_MAKE_API_VERSION(0, major, middle, minor);
	}
	return version;
}

void Application::notification(const String &title, const String &text) {
	platform::interaction::_notification(title, text);
}

void Application::setLaunchUrl(const StringView &url) {
    _data.launchUrl = url.str<Interface>();
}

void Application::scheduleUpdate() {
	if (isOnMainThread()) {
		_immediateUpdate = true;
	} else {
		performOnMainThread([this] {
			_immediateUpdate = true;
		});
	}
}

bool Application::isOnMainThread() const {
	return _threadId == std::this_thread::get_id();
}

void Application::performOnMainThread(const Function<void()> &func, Ref *target, bool onNextFrame) const {
	if (!_queue || ((isOnMainThread() || _singleThreaded) && !onNextFrame)) {
		func();
	} else {
		_queue->onMainThread(Rc<thread::Task>::create([func] (const thread::Task &, bool success) {
			if (success) { func(); }
		}, target));
	}
}

void Application::performOnMainThread(Rc<thread::Task> &&task, bool onNextFrame) const {
	if (!_queue || ((isOnMainThread() || _singleThreaded) && !onNextFrame)) {
		task->onComplete();
	} else {
		_queue->onMainThread(std::move(task));
	}
}

void Application::perform(const ExecuteCallback &exec, const CompleteCallback &complete, Ref *obj) const {
	perform(Rc<Task>::create(exec, complete, obj));
}

void Application::perform(Rc<thread::Task> &&task) const {
	if (!_queue || _singleThreaded) {
		task->setSuccessful(task->execute());
		task->onComplete();
	} else {
		_queue->perform(std::move(task));
	}
}

void Application::perform(Rc<thread::Task> &&task, bool performFirst) const {
	if (!_queue || _singleThreaded) {
		task->setSuccessful(task->execute());
		task->onComplete();
	} else {
		_queue->perform(std::move(task), performFirst);
	}
}

void Application::performAsync(Rc<Task> &&task) const {
	if (!_queue || _singleThreaded) {
		task->setSuccessful(task->execute());
		task->onComplete();
	} else {
		_queue->performAsync(std::move(task));
	}
}

void Application::performAsync(const ExecuteCallback &exec, const CompleteCallback &complete, Ref *obj) const {
	performAsync(Rc<Task>::create(exec, complete, obj));
}

void Application::setSingleThreaded(bool value) {
	_singleThreaded = value;
}

bool Application::isSingleThreaded() const {
	return _singleThreaded;
}

uint64_t Application::getNativeThreadId() const {
	return (uint64_t)pthread_self();
}

void Application::addEventListener(const EventHandlerNode *listener) {
	auto it = _eventListeners.find(listener->getEventID());
	if (it != _eventListeners.end()) {
		it->second.insert(listener);
	} else {
		_eventListeners.emplace(listener->getEventID(), std::unordered_set<const EventHandlerNode *>{listener});
	}
}
void Application::removeEventListner(const EventHandlerNode *listener) {
	auto it = _eventListeners.find(listener->getEventID());
	if (it != _eventListeners.end()) {
		it->second.erase(listener);
	}
}

void Application::removeAllListeners() {
	_eventListeners.clear();
}

void Application::dispatchEvent(const Event &ev) {
	if (_eventListeners.size() > 0) {
		auto it = _eventListeners.find(ev.getHeader().getEventID());
		if (it != _eventListeners.end() && it->second.size() != 0) {
			Vector<const EventHandlerNode *> listenersToExecute;
			auto &listeners = it->second;
			for (auto l : listeners) {
				if (l->shouldRecieveEventWithObject(ev.getEventID(), ev.getObject())) {
					listenersToExecute.push_back(l);
				}
			}

			for (auto l : listenersToExecute) {
				l->onEventRecieved(ev);
			}
		}
	}
}

void Application::sleep(uint64_t ms) {
	platform::device::_sleep(ms);
}

uint64_t Application::getClock() const {
	return platform::device::_clock(platform::device::ClockType::Monotonic);
}

const Rc<ResourceCache> &Application::getResourceCache() const {
	return _glLoop->getResourceCache();
}

void Application::updateDefaultFontController(font::FontController::Builder &builder) {

}

}
