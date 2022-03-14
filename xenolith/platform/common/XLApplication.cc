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
#include "XLNetworkController.h"
#include "XLAssetLibrary.h"
#include "XLFontFace.h"

namespace stappler::log {

void __xenolith_log(const StringView &tag, CustomLog::Type t, CustomLog::VA &va);

}

namespace stappler::xenolith {

EventLoop::EventLoop() { }

EventLoop::~EventLoop() { }

bool EventLoop::init(Application *app) {
	_application = app;
	return true;
}

bool EventLoop::run() {
	return false;
}

uint64_t EventLoop::getMinFrameTime() const {
	return 1000'000ULL / 60;
}

uint64_t EventLoop::getClock() {
	return 0;
}

void EventLoop::sleep(uint64_t) {
	// do nothing
}

Pair<uint64_t, uint64_t> EventLoop::getDiskSpace() const {
	return pair(0, 0);
}

void EventLoop::pushEvent(AppEvent::Value events) {
	_events |= events;
}

AppEvent::Value EventLoop::popEvents() {
	return _events.exchange(AppEvent::None);
}

void EventLoop::addView(gl::View *) {

}

void EventLoop::removeView(gl::View *) {

}

XL_DECLARE_EVENT_CLASS(Application, onDeviceToken);
XL_DECLARE_EVENT_CLASS(Application, onNetwork);
XL_DECLARE_EVENT_CLASS(Application, onUrlOpened);
XL_DECLARE_EVENT_CLASS(Application, onError);
XL_DECLARE_EVENT_CLASS(Application, onRemoteNotification);
XL_DECLARE_EVENT_CLASS(Application, onLaunchUrl);

static Application * s_application = nullptr;

Application *Application::getInstance() {
	return s_application;
}

int Application::parseOptionString(data::Value &ret, const StringView &str, int argc, const char * argv[]) {
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
	}
	return 1;
}

void Application::sleep(uint64_t v) {
	_loop->sleep(v);
}

uint64_t Application::getClock() const {
	return _loop->getClock();
}

Application::Application() : _appLog(&log::__xenolith_log) {
	XLASSERT(s_application == nullptr, "Application should be only one");

	memory::pool::initialize();

	_rootPool = memory::pool::create(memory::pool::acquire());
	_updatePool = memory::pool::create(_rootPool);

	_loop = platform::device::createEventLoop(this);
	_clockStart = _loop->getClock();

	_userAgent = platform::device::_userAgent();
	_deviceIdentifier  = platform::device::_deviceIdentifier();
	_isNetworkOnline = platform::network::_isNetworkOnline();

	platform::network::_setNetworkCallback([this] (bool online) {
		if (online != _isNetworkOnline) {
			setNetworkOnline(online);
		}
	});

	s_application = this;

	db::setStorageRoot(&_storageRoot);

	_networkController = Rc<network::Controller>::alloc(this, "Root");

	auto libpath = filesystem::writablePath("library");
	filesystem::mkdir(libpath);

	_assetLibrary = Rc<storage::AssetLibrary>::create(this, data::Value({
		pair("driver", data::Value("sqlite")),
		pair("dbname", data::Value(toString(libpath, "/assets.v2.db"))),
		pair("serverName", data::Value("AssetStorage"))
	}));
}

Application::~Application() {
	memory::pool::destroy(_updatePool);
	memory::pool::destroy(_rootPool);
	memory::pool::terminate();
}

bool Application::onFinishLaunching() {
	_threadId = std::this_thread::get_id();

	thread::ThreadInfo::setMainThread();

	_queue = Rc<thread::TaskQueue>::alloc("Main", [this] {
		_loop->pushEvent(AppEvent::Thread);
	});
	if (!_queue->spawnWorkers(thread::TaskQueue::Flags::None, ApplicationThreadId,
			math::clamp(uint16_t(std::thread::hardware_concurrency() / 2), uint16_t(2), uint16_t(16)), _queue->getName())) {
		log::text("Application", "Fail to spawn worker threads");
		return false;
	}

	_instance = platform::graphic::createInstance(this);

	if (!_instance) {
		_instance = nullptr;
		log::text("Application", "Fail to create vulkan instance");
		return false;
	}

	if (!_instance->hasDevices()) {
		_instance = nullptr;
		log::text("Application", "No devices for presentation found");
		return false;
	}

	auto device = _instance->makeDevice();
	_glLoop = Rc<gl::Loop>::alloc(this, device);
	_resourceCache = Rc<ResourceCache>::create(*device);
	return true;
}

bool Application::onBuildStorage(storage::Server::Builder &builder) {
	return true;
}

bool Application::onMainLoop() {
	return false;
}

void Application::onMemoryWarning() {

}

int Application::run(data::Value &&data) {
	memory::pool::push(_updatePool);
	_dbParams = data::Value({
		pair("driver", data::Value("sqlite")),
		pair("dbname", data::Value(filesystem::cachesPath("root.sqlite"))),
		pair("serverName", data::Value("RootStorage"))
	});

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
		}
	}

	_storageServer = Rc<storage::Server>::create(this, _dbParams, [&] (storage::Server::Builder &builder) {
		return onBuildStorage(builder);
	});

	if (!_storageServer) {
		log::text("Application", "Fail to launch application: onBuildStorage failed");
		memory::pool::pop();
		return 1;
	}

	if (!onFinishLaunching()) {
		log::text("Application", "Fail to launch application: onFinishLaunching failed");
		memory::pool::pop();
		return 1;
	}

	_glLoop->begin();
	auto ret = onMainLoop();

	_resourceCache->invalidate(*_glLoop->getDevice());
	_glLoop->end();
	_glLoop = nullptr;

	_instance = nullptr;

	memory::pool::pop();
    return ret ? 0 : -1;
}

bool Application::openURL(const StringView &url) {
	return platform::interaction::_goToUrl(url, true);
}

void Application::update(uint64_t dt) {
	updateQueue();

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

	memory::pool::pop();
	memory::pool::clear(_updatePool);
}

void Application::updateQueue() {
	memory::pool::push(_updatePool);
	if (_queue) {
		_queue->update();
	}
	memory::pool::pop();
	memory::pool::clear(_updatePool);
}

void Application::registerDeviceToken(const uint8_t *data, size_t len) {
    registerDeviceToken(base16::encode(CoderSource(data, len)));
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

std::pair<uint64_t, uint64_t> Application::getTotalDiskSpace() {
	return _loop->getDiskSpace();
}

uint64_t Application::getApplicationDiskSpace() {
	auto path = filesystem::writablePath(_data.bundleName);
	uint64_t size = 0;
	filesystem::ftw(path, [&size] (const StringView &path, bool isFile) {
		if (isFile) {
			size += filesystem::size(path);
		}
	});

	path = filesystem::cachesPath(_data.bundleName);
	filesystem::ftw(path, [&size] (const StringView &path, bool isFile) {
		if (isFile) {
			size += filesystem::size(path);
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
    _data.launchUrl = url.str();
}

void Application::processLaunchUrl(const StringView &url) {
	_data.launchUrl = url.str();
    onLaunchUrl(this, url);
}

bool Application::isMainThread() const {
	return _threadId == std::this_thread::get_id();
}

void Application::performOnMainThread(const Function<void()> &func, Ref *target, bool onNextFrame) const {
	if (!_queue || ((isMainThread() || _singleThreaded) && !onNextFrame)) {
		func();
	} else {
		_queue->onMainThread(Rc<thread::Task>::create([func] (const thread::Task &, bool success) {
			if (success) { func(); }
		}, target));
	}
}

void Application::performOnMainThread(Rc<thread::Task> &&task, bool onNextFrame) const {
	if (!_queue || ((isMainThread() || _singleThreaded) && !onNextFrame)) {
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

}
