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

#include "XLPlatform.h"
#include "SPFilesystem.h"

#if (LINUX)

#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#include "XLApplication.h"

namespace stappler::xenolith::platform::device {

class EventLoopLinux : public EventLoop {
public:
	struct ViewFdData {
		Rc<gl::View> view;
		bool isEventFd = false;
		struct epoll_event event;
	};

	virtual ~EventLoopLinux();

	virtual bool init(Application *app) override;
	virtual bool run() override;

	virtual void addView(gl::View *) override;
	virtual void removeView(gl::View *) override;

	virtual uint64_t getMinFrameTime() const override;
	virtual uint64_t getClock() override;
	virtual void sleep(uint64_t) override;

	virtual Pair<uint64_t, uint64_t> getDiskSpace() const override;

	virtual void pushEvent(AppEvent::Value val) override;

protected:
	int _epollFd = -1;
	int _eventFd = -1;
	struct epoll_event _eventEvent;
	std::unordered_map<int, ViewFdData> _fds;
};

EventLoopLinux::~EventLoopLinux() {
	if (_epollFd >= 0) {
		::close(_epollFd);
		_epollFd = -1;
	}

	if (_eventFd >= 0) {
		::close(_eventFd);
		_eventFd = -1;
	}
}

bool EventLoopLinux::init(Application *app) {
	if (!EventLoop::init(app)) {
		return false;
	}
	_eventFd = eventfd(0, EFD_NONBLOCK);
	_eventEvent.data.fd = _eventFd;
	_eventEvent.events = EPOLLIN | EPOLLET;

	_epollFd = ::epoll_create1(0);
	auto err = ::epoll_ctl(_epollFd, EPOLL_CTL_ADD, _eventFd, &_eventEvent);
	if (err == -1) {
		char buf[256] = { 0 };
		stappler::log::vtext("EventLoopLinux", "Fail to start thread worker: epoll_ctl(",
				_eventFd, ", EPOLL_CTL_ADD): ", strerror_r(errno, buf, 255), "\n");
	}

	return true;
}

bool EventLoopLinux::run() {
	static constexpr auto MaxEvents = 8;

	bool _shouldClose = false;
	std::array<struct epoll_event, MaxEvents> _events;

	while (!_shouldClose) {
		int nevents = ::epoll_wait(_epollFd, _events.data(), MaxEvents, -1);
		if (nevents == -1 && errno != EINTR) {
			char buf[256] = { 0 };
			log::vtext("EventLoopLinux", "epoll_wait() failed with errno ", errno, " (", strerror_r(errno, buf, 255), ")");
			return false;
		} else if (nevents <= 0 && errno == EINTR) {
			continue;
		}

		for (int i = 0; i < nevents; i++) {
			auto fd = _events[i].data.fd;
			if ((_events[i].events & EPOLLERR)) {
				log::vtext("EventLoopLinux", "epoll error on fd: ", fd);
				continue;
			}
			if ((_events[i].events & EPOLLIN)) {
				if (fd == _eventFd) {
					uint64_t value = 0;
					auto sz = read(_eventFd, &value, sizeof(uint64_t));
					if (sz == 8 && value) {
						// received app event
						auto evVal = popEvents();
						if ((evVal & AppEvent::Terminate) != 0) {
							_shouldClose = true;
							continue;
						}
						if ((evVal & AppEvent::Thread) != 0) {
							_application->updateQueue();
						}
					}
				} else {
					auto d = _fds.find(fd);
					if (d != _fds.end()) {
						if (d->second.isEventFd) {
							auto evVal = d->second.view->popEvents();
							if ((evVal & AppEvent::Terminate) != 0) {
								d->second.view->close();
								continue;
							}
							if ((evVal & AppEvent::SwapchainRecreationBest) != 0) {
								d->second.view->reset(gl::SwapchanCreationMode::Best);
							} else if ((evVal & AppEvent::SwapchainRecreation) != 0) {
								d->second.view->reset(gl::SwapchanCreationMode::Fast);
							}
							if ((evVal & AppEvent::Update) != 0) {
								d->second.view->update();
							}
							if ((evVal & AppEvent::Input) != 0) {
								// manual input event
								d->second.view->poll();
							}
						} else {
							// socket has input
							if (!d->second.view->poll()) {
								d->second.view->close();
								continue;
							}
						}
					}
				}
			}
		}
	}

	return !_shouldClose;
}

void EventLoopLinux::addView(gl::View *view) {
	auto v = ((vk::ViewImpl *)view)->getView();
	auto eventFd = v->getEventFd();
	if (eventFd >= 0) {
		auto it = _fds.emplace(eventFd, ViewFdData({view, true})).first;

		it->second.event.data.fd = eventFd;
		it->second.event.events = EPOLLIN | EPOLLET;

		auto err = ::epoll_ctl(_epollFd, EPOLL_CTL_ADD, eventFd, &it->second.event);
		if (err == -1) {
			char buf[256] = { 0 };
			stappler::log::vtext("EventLoopLinux", "Fail to add view: epoll_ctl(",
					eventFd, ", EPOLL_CTL_ADD): ", strerror_r(errno, buf, 255), "\n");
		}
	}

	auto socketFd = v->getSocketFd();
	if (socketFd >= 0) {
		auto it = _fds.emplace(socketFd, ViewFdData({view, false})).first;

		it->second.event.data.fd = socketFd;
		it->second.event.events = EPOLLIN;

		auto err = ::epoll_ctl(_epollFd, EPOLL_CTL_ADD, socketFd, &it->second.event);
		if (err == -1) {
			char buf[256] = { 0 };
			stappler::log::vtext("EventLoopLinux", "Fail to add view: epoll_ctl(",
					socketFd, ", EPOLL_CTL_ADD): ", strerror_r(errno, buf, 255), "\n");
		}
	}
}

void EventLoopLinux::removeView(gl::View *view) {
	auto it = _fds.begin();
	while (it != _fds.end()) {
		if (it->second.view == view) {
			 ::epoll_ctl(_epollFd, EPOLL_CTL_DEL, it->second.event.data.fd, &it->second.event);
			it = _fds.erase(it);
		} else {
			++ it;
		}
	}
}

void EventLoopLinux::sleep(uint64_t microseconds) {
	usleep(useconds_t(microseconds));
}

static uint64_t getStaticMinFrameTime() {
	return 1000'000 / 60;
}

uint64_t EventLoopLinux::getMinFrameTime() const {
	return getStaticMinFrameTime();
}

static clockid_t getClockSource() {
	struct timespec ts;

	auto minFrameNano = (getStaticMinFrameTime() * 1000) / 5; // clock should have at least 1/5 frame resolution
	if (clock_getres(CLOCK_MONOTONIC_COARSE, &ts) == 0) {
		if (ts.tv_sec == 0 && uint64_t(ts.tv_nsec) < minFrameNano) {
			return CLOCK_MONOTONIC_COARSE;
		}
	}

	if (clock_getres(CLOCK_MONOTONIC, &ts) == 0) {
		if (ts.tv_sec == 0 && uint64_t(ts.tv_nsec) < minFrameNano) {
			return CLOCK_MONOTONIC;
		}
	}

	if (clock_getres(CLOCK_MONOTONIC_RAW, &ts) == 0) {
		if (ts.tv_sec == 0 && uint64_t(ts.tv_nsec) < minFrameNano) {
			return CLOCK_MONOTONIC_RAW;
		}
	}

	return CLOCK_MONOTONIC;
}

uint64_t EventLoopLinux::getClock() {
	static clockid_t ClockSource = getClockSource();

	struct timespec ts;
	clock_gettime(ClockSource, &ts);
	return (uint64_t) ts.tv_sec * (uint64_t) 1000'000 + (uint64_t) ts.tv_nsec / 1000;
}

Pair<uint64_t, uint64_t> EventLoopLinux::getDiskSpace() const {
	uint64_t totalSpace = 0;
	uint64_t totalFreeSpace = 0;

	return pair(totalSpace, totalFreeSpace);
}

void EventLoopLinux::pushEvent(AppEvent::Value val) {
	EventLoop::pushEvent(val);
	uint64_t value = 1;
	write(_eventFd, &value, sizeof(uint64_t));
}

String _userAgent() {
	return "Mozilla/5.0 (Linux;)";
}

String _deviceIdentifier() {
	auto path = stappler::platform::filesystem::_getCachesPath();
	auto devIdPath = path + "/.devid";
	if (stappler::filesystem::exists(devIdPath)) {
		auto data = stappler::filesystem::readIntoMemory(devIdPath);
		return base16::encode(data);
	} else {
		Bytes data; data.resize(16);
		auto fp = fopen("/dev/urandom", "r");
		if (fread(data.data(), 1, data.size(), fp) == 0) {
			log::text("Device", "Fail to read random bytes");
		}
		fclose(fp);

		stappler::filesystem::write(devIdPath, data);
		return base16::encode(data);
	}
}

uint64_t _clock() {
	static clockid_t ClockSource = getClockSource();

	struct timespec ts;
	clock_gettime(ClockSource, &ts);
	return (uint64_t) ts.tv_sec * (uint64_t) 1000'000 + (uint64_t) ts.tv_nsec / 1000;
}

Rc<EventLoop> createEventLoop(Application *app) {
	return Rc<EventLoopLinux>::create(app);
}

}

#endif
