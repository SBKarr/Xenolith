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

#include "XLPlatform.h"

#if ANDROID

#include "XLPlatformAndroid.h"
#include <sys/timerfd.h>
#include <android/hardware_buffer.h>
#include <dlfcn.h>

namespace stappler::xenolith::platform {

EngineMainThread::~EngineMainThread() {
	_application->end();
	_thread.join();
}

bool EngineMainThread::init(Application *app, Value &&args) {
	_application = app;
	_thread = std::thread(EngineMainThread::workerThread, this, nullptr);
	_args = move(args);
	return true;
}

void EngineMainThread::threadInit() {
	_threadId = std::this_thread::get_id();
}

void EngineMainThread::threadDispose() {

}

bool EngineMainThread::worker() {
	_application->run(move(_args), [&] (Application *app) {
		_running.store(true);
		std::unique_lock lock(_runningMutex);
		_runningVar.notify_all();
	});
	return false;
}

void EngineMainThread::waitForRunning() {
	if (_running.load()) {
		return;
	}

	std::unique_lock lock(_runningMutex);
	_runningVar.wait(lock, [&] {
		return _running.load();
	});
}

static std::atomic<NativeActivity *> s_currentActivity = nullptr;

NativeActivity *NativeActivity::getInstance() {
	return s_currentActivity;
}

NativeActivity::~NativeActivity() {
	if (s_currentActivity == this) {
		s_currentActivity = nullptr;
	}

	if (rootView) {
		rootView->threadDispose();
		rootView->end();
		rootView = nullptr;
	}

	if (networkConnectivity) {
		networkConnectivity->finalize(activity->env);
		networkConnectivity = nullptr;
	}

	if (classLoader) {
		classLoader->finalize(activity->env);
		classLoader = nullptr;
	}

	filesystem::platform::Android_terminateFilesystem();
	auto app = thread->getApplication();
	app->end(true);

	thread->getApplication()->setNativeHandle(nullptr);
	thread = nullptr;

	if (looper) {
		if (_eventfd >= 0) {
			ALooper_removeFd(looper, _eventfd);
		}
		if (_timerfd >= 0) {
			ALooper_removeFd(looper, _timerfd);
		}
		ALooper_release(looper);
		looper = nullptr;
	}
	if (config) {
		AConfiguration_delete(config);
		config = nullptr;
	}
	if (_eventfd) {
		::close(_eventfd);
		_eventfd = -1;
	}
	if (_timerfd) {
		::close(_timerfd);
		_timerfd = -1;
	}
}

bool NativeActivity::init(ANativeActivity *a) {
	activity = a;
	config = AConfiguration_new();
	AConfiguration_fromAssetManager(config, activity->assetManager);
	sdkVersion = AConfiguration_getSdkVersion(config);

	if (sdkVersion >= 29) {
		auto handle = ::dlopen(nullptr, RTLD_LAZY);
		if (handle) {
			auto fn_AHardwareBuffer_isSupported = ::dlsym(handle, "AHardwareBuffer_isSupported");
			if (fn_AHardwareBuffer_isSupported) {
				auto _AHardwareBuffer_isSupported = (int (*) (const AHardwareBuffer_Desc *))fn_AHardwareBuffer_isSupported;

				// check for common buffer formats
				auto checkSupported = [&] (int format) -> bool {
					AHardwareBuffer_Desc desc;
					desc.width = 1024;
					desc.height = 1024;
					desc.layers = 1;
					desc.format = format;
					desc.usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
					desc.stride = 0;
					desc.rfu0 = 0;
					desc.rfu1 = 0;

					return _AHardwareBuffer_isSupported(&desc) != 0;
				};

				formatSupport = NativeBufferFormatSupport{
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM),
					checkSupported(AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT),
					checkSupported(AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM)
				};
			}
		}
	}

	looper = ALooper_forThread();
	if (looper) {
		ALooper_acquire(looper);

		_eventfd = ::eventfd(0, EFD_NONBLOCK);

		ALooper_addFd(looper, _eventfd, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT,
				[] (int fd, int events, void* data) -> int {
			return ((NativeActivity *)data)->handleLooperEvent(fd, events);
		}, this);

		struct itimerspec timer{
			{ 0, config::PresentationSchedulerInterval * 1000 },
			{ 0, config::PresentationSchedulerInterval * 1000 }
		};

		_timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
		::timerfd_settime(_timerfd, 0, &timer, nullptr);

		ALooper_addFd(looper, _timerfd, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT,
				[] (int fd, int events, void* data) -> int {
			return ((NativeActivity *)data)->handleLooperEvent(fd, events);
		}, this);
	}

	activity->callbacks->onConfigurationChanged = [] (ANativeActivity* a) {
		((NativeActivity *)a->instance)->handleConfigurationChanged();
	};

	activity->callbacks->onContentRectChanged = [] (ANativeActivity* a, const ARect* r) {
		((NativeActivity *)a->instance)->handleContentRectChanged(r);
	};

	activity->callbacks->onDestroy = [] (ANativeActivity* a) {
		stappler::log::format("NativeActivity", "Destroy: %p", a);

		auto ref = (NativeActivity *)a->instance;
		delete ref;
	};
	activity->callbacks->onInputQueueCreated = [] (ANativeActivity* a, AInputQueue* queue) {
		((NativeActivity *)a->instance)->handleInputQueueCreated(queue);
	};
	activity->callbacks->onInputQueueDestroyed = [] (ANativeActivity* a, AInputQueue* queue) {
		((NativeActivity *)a->instance)->handleInputQueueDestroyed(queue);
	};
	activity->callbacks->onLowMemory = [] (ANativeActivity* a) {
		((NativeActivity *)a->instance)->handleLowMemory();
	};

	activity->callbacks->onNativeWindowCreated = [] (ANativeActivity* a, ANativeWindow* window) {
		((NativeActivity *)a->instance)->handleNativeWindowCreated(window);
	};

	activity->callbacks->onNativeWindowDestroyed = [] (ANativeActivity* a, ANativeWindow* window) {
		((NativeActivity *)a->instance)->handleNativeWindowDestroyed(window);
	};

	activity->callbacks->onNativeWindowRedrawNeeded = [] (ANativeActivity* a, ANativeWindow* window) {
		((NativeActivity *)a->instance)->handleNativeWindowRedrawNeeded(window);
	};

	activity->callbacks->onNativeWindowResized = [] (ANativeActivity* a, ANativeWindow* window) {
		((NativeActivity *)a->instance)->handleNativeWindowResized(window);
	};
	activity->callbacks->onPause = [] (ANativeActivity* a) {
		((NativeActivity *)a->instance)->handlePause();
	};
	activity->callbacks->onResume = [] (ANativeActivity* a) {
		((NativeActivity *)a->instance)->handleResume();
	};
	activity->callbacks->onSaveInstanceState = [] (ANativeActivity* a, size_t* outLen) {
		return ((NativeActivity *)a->instance)->handleSaveInstanceState(outLen);
	};
	activity->callbacks->onStart = [] (ANativeActivity* a) {
		((NativeActivity *)a->instance)->handleStart();
	};
	activity->callbacks->onStop = [] (ANativeActivity* a) {
		((NativeActivity *)a->instance)->handleStop();
	};
	activity->callbacks->onWindowFocusChanged = [] (ANativeActivity *a, int focused) {
		((NativeActivity *)a->instance)->handleWindowFocusChanged(focused);
	};

	auto activityClass = activity->env->GetObjectClass(activity->clazz);
	auto setNativePointerMethod = activity->env->GetMethodID(activityClass, "setNativePointer", "(J)V");
	if (setNativePointerMethod) {
		activity->env->CallVoidMethod(activity->clazz, setNativePointerMethod, jlong(this));
	}

	checkJniError(activity->env);

	auto isEmulatorMethod = activity->env->GetMethodID(activityClass, "isEmulator", "()Z");
	if (isEmulatorMethod) {
		isEmulator = activity->env->CallBooleanMethod(activity->clazz, isEmulatorMethod);
		if (isEmulator) {
			// emulators often do not support this format for swapchains
			formatSupport.R8G8B8A8_UNORM = false;
			stappler::xenolith::platform::graphic::s_getCommonFormat = gl::ImageFormat::R5G6B5_UNORM_PACK16;
		}
	}

	checkJniError(activity->env);

	activity->env->DeleteLocalRef(activityClass);

	classLoader = Rc<NativeClassLoader>::create(activity);

	filesystem::platform::Android_initializeFilesystem(activity->assetManager,
			activity->internalDataPath, activity->externalDataPath, classLoader ? classLoader->apkPath : StringView());

	activity->instance = this;

	auto app = Application::getInstance();
	app->setNativeHandle(this);

	if (classLoader) {
		networkConnectivity = Rc<NetworkConnectivity>::create(activity->env, classLoader, activity->clazz, [this] (NetworkCapabilities flags) {
			if (!thread) {
				return;
			}
			thread->getApplication()->performOnMainThread([this, flags] {
				if ((flags & NetworkCapabilities::NET_CAPABILITY_INTERNET) != NetworkCapabilities::None) {
					platform::network::Android_setNetworkOnline(true);
					thread->getApplication()->setNetworkOnline(true);
				} else {
					platform::network::Android_setNetworkOnline(false);
					thread->getApplication()->setNetworkOnline(false);
				}
			});
		});
		if (networkConnectivity) {
			if ((networkConnectivity->capabilities & NetworkCapabilities::NET_CAPABILITY_INTERNET) != NetworkCapabilities::None) {
				platform::network::Android_setNetworkOnline(true);
				app->setNetworkOnline(true);
			} else {
				platform::network::Android_setNetworkOnline(false);
				app->setNetworkOnline(false);
			}
		}
	}

	s_currentActivity = this;

	thread = Rc<EngineMainThread>::create(app, getAppInfo(config));
	return true;
}

void NativeActivity::wakeup() {
	uint64_t value = 1;
	::write(_eventfd, &value, sizeof(value));
}

void NativeActivity::setView(graphic::ViewImpl *view) {
	std::unique_lock lock(rootViewMutex);
	rootViewTmp = view;
	rootViewVar.notify_all();
}

void NativeActivity::handleConfigurationChanged() {
	if (config) {
		AConfiguration_delete(config);
	}
	config = AConfiguration_new();
	AConfiguration_fromAssetManager(config, activity->assetManager);
	sdkVersion = AConfiguration_getSdkVersion(config);

	auto appInfo = getAppInfo(config);

	auto app = thread->getApplication().get();
	app->performOnMainThread([appInfo = move(appInfo), app] () mutable {
		app->updateConfig(move(appInfo));
	}, app);

	stappler::log::format("NativeActivity", "onConfigurationChanged");
}

void NativeActivity::handleContentRectChanged(const ARect *r) {
	if (auto &view = waitForView()) {
		auto verticalSpace = _windowSize.height - r->top;
		auto top = r->top - (r->bottom - verticalSpace);
		auto bottom = r->bottom - verticalSpace;
		view->setContentPadding(Padding(r->top, _windowSize.width - r->right, _windowSize.height - r->bottom, r->left));
	}

	stappler::log::format("NativeActivity", "ContentRectChanged: l=%d,t=%d,r=%d,b=%d", r->left, r->top, r->right, r->bottom);
}

void NativeActivity::handleInputQueueCreated(AInputQueue *queue) {
	auto it = _input.emplace(queue, InputLooperData{this, queue}).first;
	AInputQueue_attachLooper(queue, looper, 0, [] (int fd, int events, void* data) {
		auto d = (InputLooperData *)data;
		return d->activity->handleInputEventQueue(fd, events, d->queue);
	}, &it->second);
}

void NativeActivity::handleInputQueueDestroyed(AInputQueue *queue) {
	AInputQueue_detachLooper(queue);
	_input.erase(queue);
}

void NativeActivity::handleLowMemory() {
	stappler::log::format("NativeActivity", "onLowMemory");
}

void *NativeActivity::handleSaveInstanceState(size_t* outLen) {
	stappler::log::format("NativeActivity", "onSaveInstanceState");
	return nullptr;
}

void NativeActivity::handleNativeWindowCreated(ANativeWindow *window) {
    stappler::log::format("NativeActivity", "NativeWindowCreated: %p -- %p -- %d x %d", activity, window,
			ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

	thread->waitForRunning();

	if (auto &view = waitForView()) {
		view->runWithWindow(window);
	}

	_windowSize = Size2(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
}

void NativeActivity::handleNativeWindowDestroyed(ANativeWindow *window) {
	if (rootView) {
		rootView->stopWindow();
	}

	stappler::log::format("NativeActivity", "NativeWindowDestroyed: %p -- %p", activity, window);
}

void NativeActivity::handleNativeWindowRedrawNeeded(ANativeWindow *window) {
	if (rootView) {
		rootView->setReadyForNextFrame();
		rootView->update(true);
	}

	stappler::log::format("NativeActivity", "NativeWindowRedrawNeeded: %p -- %p", activity, window);
}

void NativeActivity::handleNativeWindowResized(ANativeWindow *window) {
	stappler::log::format("NativeActivity", "NativeWindowResized: %p -- %p -- %d x %d", activity, window,
			ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

	_windowSize = Size2(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
	if (rootView) {
		rootView->deprecateSwapchain(false);
	}
}

void NativeActivity::handlePause() {
	InputEventData event = InputEventData::BoolEvent(InputEventName::Background, true);
	if (rootView) {
		rootView->handleInputEvent(event);
	}
}

void NativeActivity::handleStart() {
	stappler::log::format("NativeActivity", "onStart");
}

void NativeActivity::handleResume() {
	InputEventData event = InputEventData::BoolEvent(InputEventName::Background, false);
	if (rootView) {
		rootView->handleInputEvent(event);
	}
}

void NativeActivity::handleStop() {
	stappler::log::format("NativeActivity", "onStop");
}

void NativeActivity::handleWindowFocusChanged(int focused) {
	InputEventData event = InputEventData::BoolEvent(InputEventName::FocusGain, focused != 0);
	if (rootView) {
		rootView->handleInputEvent(event);
	}
}

int NativeActivity::handleLooperEvent(int fd, int events) {
	if (fd == _eventfd && events == ALOOPER_EVENT_INPUT) {
		uint64_t value = 0;
		::read(fd, &value, sizeof(value));
		if (value > 0) {
			if (rootView) {
				rootView->update(false);
			}
		}
		return 1;
	} else if (fd == _timerfd && events == ALOOPER_EVENT_INPUT) {
		if (rootView) {
			rootView->update(false);
		}
		return 1;
	}
	return 0;
}

int NativeActivity::handleInputEventQueue(int fd, int events, AInputQueue *queue) {
    AInputEvent* event = NULL;
    while (AInputQueue_getEvent(queue, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(queue, event)) {
            continue;
        }
        int32_t handled = handleInputEvent(event);
        AInputQueue_finishEvent(queue, event, handled);
    }
    return 1;
}

int NativeActivity::handleInputEvent(AInputEvent *event) {
	auto type = AInputEvent_getType(event);
	auto source = AInputEvent_getSource(event);
	switch (type) {
	case AINPUT_EVENT_TYPE_KEY:
		return handleKeyEvent(event);
		break;
	case AINPUT_EVENT_TYPE_MOTION:
		return handleMotionEvent(event);
		break;
	}
	return 0;
}

int NativeActivity::handleKeyEvent(AInputEvent *event) {
	auto action = AKeyEvent_getAction(event);
	auto flags = AKeyEvent_getFlags(event);
	auto modsFlags = AKeyEvent_getMetaState(event);

	auto keyCode = AKeyEvent_getKeyCode(event);
	auto scanCode = AKeyEvent_getKeyCode(event);

	if (keyCode == AKEYCODE_BACK) {
		if (rootView->getBackButtonCounter() == 0) {
			return 0;
		}
	}

	InputModifier mods = InputModifier::None;
	if (modsFlags != AMETA_NONE) {
		if ((modsFlags & AMETA_ALT_ON) != AMETA_NONE) { mods |= InputModifier::Alt; }
		if ((modsFlags & AMETA_ALT_LEFT_ON) != AMETA_NONE) { mods |= InputModifier::AltL; }
		if ((modsFlags & AMETA_ALT_RIGHT_ON) != AMETA_NONE) { mods |= InputModifier::AltR; }
		if ((modsFlags & AMETA_SHIFT_ON) != AMETA_NONE) { mods |= InputModifier::Shift; }
		if ((modsFlags & AMETA_SHIFT_LEFT_ON) != AMETA_NONE) { mods |= InputModifier::ShiftL; }
		if ((modsFlags & AMETA_SHIFT_RIGHT_ON) != AMETA_NONE) { mods |= InputModifier::ShiftR; }
		if ((modsFlags & AMETA_CTRL_ON) != AMETA_NONE) { mods |= InputModifier::Ctrl; }
		if ((modsFlags & AMETA_CTRL_LEFT_ON) != AMETA_NONE) { mods |= InputModifier::CtrlL; }
		if ((modsFlags & AMETA_CTRL_RIGHT_ON) != AMETA_NONE) { mods |= InputModifier::CtrlR; }
		if ((modsFlags & AMETA_META_ON) != AMETA_NONE) { mods |= InputModifier::Mod3; }
		if ((modsFlags & AMETA_META_LEFT_ON) != AMETA_NONE) { mods |= InputModifier::Mod3L; }
		if ((modsFlags & AMETA_META_RIGHT_ON) != AMETA_NONE) { mods |= InputModifier::Mod3R; }

		if ((modsFlags & AMETA_CAPS_LOCK_ON) != AMETA_NONE) { mods |= InputModifier::CapsLock; }
		if ((modsFlags & AMETA_NUM_LOCK_ON) != AMETA_NONE) { mods |= InputModifier::NumLock; }

		if ((modsFlags & AMETA_SCROLL_LOCK_ON) != AMETA_NONE) { mods |= InputModifier::ScrollLock; }
		if ((modsFlags & AMETA_SYM_ON) != AMETA_NONE) { mods |= InputModifier::Sym; }
		if ((modsFlags & AMETA_FUNCTION_ON) != AMETA_NONE) { mods |= InputModifier::Function; }
	}

	_activeModifiers = mods;

	Vector<InputEventData> events;

	bool isCanceled = (flags & AKEY_EVENT_FLAG_CANCELED) != 0 | (flags & AKEY_EVENT_FLAG_CANCELED_LONG_PRESS) != 0;

	switch (action) {
	case AKEY_EVENT_ACTION_DOWN: {
		auto &ev = events.emplace_back(InputEventData{uint32_t(keyCode),
			InputEventName::KeyPressed, InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	case AKEY_EVENT_ACTION_UP: {
		auto &ev = events.emplace_back(InputEventData{uint32_t(keyCode),
			isCanceled ? InputEventName::KeyCanceled : InputEventName::KeyReleased, InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	case AKEY_EVENT_ACTION_MULTIPLE: {
		auto &ev = events.emplace_back(InputEventData{uint32_t(keyCode),
			InputEventName::KeyRepeated, InputMouseButton::Touch, _activeModifiers,
			_hoverLocation.x, _hoverLocation.y});
		ev.key.keycode = s_keycodes[keyCode];
		ev.key.compose = InputKeyComposeState::Nothing;
		ev.key.keysym = keyCode;
		ev.key.keychar = 0;
		break;
	}
	}
	if (!events.empty()) {
		if (rootView) {
			rootView->handleInputEvents(move(events));
		}
		return 1;
	}
	return 0;
}

int NativeActivity::handleMotionEvent(AInputEvent *event) {
	Vector<InputEventData> events;
	auto action = AMotionEvent_getAction(event);
	auto count = AMotionEvent_getPointerCount(event);
	switch (action & AMOTION_EVENT_ACTION_MASK) {
	case AMOTION_EVENT_ACTION_DOWN:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_DOWN ", count,
							 " ", AMotionEvent_getPointerId(event, 0), " ", 0);
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				InputEventName::Begin, InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_UP:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_UP ", count,
							 " ", AMotionEvent_getPointerId(event, 0), " ", 0);
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				InputEventName::End, InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_MOVE:
		for (size_t i = 0; i < count; ++ i) {
			if (AMotionEvent_getHistorySize(event) == 0
					|| AMotionEvent_getX(event, i) - AMotionEvent_getHistoricalX(event, i, AMotionEvent_getHistorySize(event) - 1) != 0.0f
					|| AMotionEvent_getY(event, i) - AMotionEvent_getHistoricalY(event, i, AMotionEvent_getHistorySize(event) - 1) != 0.0f) {
				auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
					InputEventName::Move, InputMouseButton::Touch, _activeModifiers,
					AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
				ev.point.density = _density;
			}
		}
		break;
	case AMOTION_EVENT_ACTION_CANCEL:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				InputEventName::Cancel, InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
		}
		break;
	case AMOTION_EVENT_ACTION_OUTSIDE:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_OUTSIDE ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_POINTER_DOWN: {
		auto pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8;
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_POINTER_DOWN ", count,
							 " ", AMotionEvent_getPointerId(event, pointer), " ", pointer);
		auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, pointer)),
			InputEventName::Begin, InputMouseButton::Touch, _activeModifiers,
			AMotionEvent_getX(event, pointer), _windowSize.height - AMotionEvent_getY(event, pointer)});
		ev.point.density = _density;
		break;
	}
	case AMOTION_EVENT_ACTION_POINTER_UP: {
		auto pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> 8;
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_POINTER_UP ", count,
							 " ", AMotionEvent_getPointerId(event, pointer), " ", pointer);
		auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, pointer)),
			InputEventName::End, InputMouseButton::Touch, _activeModifiers,
			AMotionEvent_getX(event, pointer), _windowSize.height - AMotionEvent_getY(event, pointer)});
		ev.point.density = _density;
		break;
	}
	case AMOTION_EVENT_ACTION_HOVER_MOVE:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData{uint32_t(AMotionEvent_getPointerId(event, i)),
				InputEventName::MouseMove, InputMouseButton::Touch, _activeModifiers,
				AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i)});
			ev.point.density = _density;
			_hoverLocation = Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i));
		}
		break;
	case AMOTION_EVENT_ACTION_SCROLL:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_SCROLL ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_HOVER_ENTER:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData::BoolEvent(InputEventName::PointerEnter, true,
					Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i))));
			ev.id = uint32_t(AMotionEvent_getPointerId(event, i));
			ev.point.density = _density;
		}
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_HOVER_ENTER ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_HOVER_EXIT:
		for (size_t i = 0; i < count; ++ i) {
			auto &ev = events.emplace_back(InputEventData::BoolEvent(InputEventName::PointerEnter, false,
					Vec2(AMotionEvent_getX(event, i), _windowSize.height - AMotionEvent_getY(event, i))));
			ev.id = uint32_t(AMotionEvent_getPointerId(event, i));
			ev.point.density = _density;
		}
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_HOVER_EXIT ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_BUTTON_PRESS:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_BUTTON_PRESS ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	case AMOTION_EVENT_ACTION_BUTTON_RELEASE:
		stappler::log::vtext("NativeActivity", "Motion AMOTION_EVENT_ACTION_BUTTON_RELEASE ", count,
				" ", AMotionEvent_getPointerId(event, 0));
		break;
	}
	if (!events.empty()) {
		if (rootView) {
			rootView->handleInputEvents(move(events));
		}
		return 1;
	}
	return 0;
}

Value NativeActivity::getAppInfo(AConfiguration *config) {
	Value appInfo;

	auto cl = getClass();

	auto j_getPackageName = getMethodID(cl, "getPackageName", "()Ljava/lang/String;");
	if (jstring strObj = (jstring) activity->env->CallObjectMethod(activity->clazz, j_getPackageName)) {
		appInfo.setString(activity->env->GetStringUTFChars(strObj, NULL), "bundle");
	}

	int widthPixels = 0;
	int heightPixels = 0;
	float density = nan();
	auto j_getResources = getMethodID(cl, "getResources", "()Landroid/content/res/Resources;");
	if (jobject resObj = (jobject) activity->env->CallObjectMethod(activity->clazz, j_getResources)) {
		auto j_getDisplayMetrics = getMethodID(activity->env->GetObjectClass(resObj),
			"getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
		if (jobject dmObj = (jobject) activity->env->CallObjectMethod(resObj, j_getDisplayMetrics)) {
			auto j_density = activity->env->GetFieldID(activity->env->GetObjectClass(dmObj), "density", "F");
			auto j_heightPixels = activity->env->GetFieldID(activity->env->GetObjectClass(dmObj), "heightPixels", "I");
			auto j_widthPixels = activity->env->GetFieldID(activity->env->GetObjectClass(dmObj), "widthPixels", "I");
			density = activity->env->GetFloatField(dmObj, j_density);
			heightPixels = activity->env->GetIntField(dmObj, j_heightPixels);
			widthPixels = activity->env->GetIntField(dmObj, j_widthPixels);
		}
	}

	String language = "en-us";
	AConfiguration_getLanguage(config, language.data());
	AConfiguration_getCountry(config, language.data() + 3);
	string::tolower(language);
	appInfo.setString(language, "locale");

	if (isnan(density)) {
		auto densityValue = AConfiguration_getDensity(config);
		switch (densityValue) {
			case ACONFIGURATION_DENSITY_LOW: density = 0.75f; break;
			case ACONFIGURATION_DENSITY_MEDIUM: density = 1.0f; break;
			case ACONFIGURATION_DENSITY_TV: density = 1.5f; break;
			case ACONFIGURATION_DENSITY_HIGH: density = 1.5f; break;
			case 280: density = 2.0f; break;
			case ACONFIGURATION_DENSITY_XHIGH: density = 2.0f; break;
			case 360: density = 3.0f; break;
			case 400: density = 3.0f; break;
			case 420: density = 3.0f; break;
			case ACONFIGURATION_DENSITY_XXHIGH: density = 3.0f; break;
			case 560: density = 4.0f; break;
			case ACONFIGURATION_DENSITY_XXXHIGH: density = 4.0f; break;
			default: break;
		}
	}

	appInfo.setDouble(density, "density");
	_density = density;

	int32_t orientation = AConfiguration_getOrientation(config);
	int32_t width = AConfiguration_getScreenWidthDp(config);
	int32_t height = AConfiguration_getScreenHeightDp(config);

	switch (orientation) {
	case ACONFIGURATION_ORIENTATION_ANY:
	case ACONFIGURATION_ORIENTATION_SQUARE:
		appInfo.setDouble(widthPixels / density, "width");
		appInfo.setDouble(heightPixels / density, "height");
		break;
	case ACONFIGURATION_ORIENTATION_PORT:
		appInfo.setDouble(std::min(widthPixels, heightPixels) / density, "width");
		appInfo.setDouble(std::max(widthPixels, heightPixels) / density, "height");
		break;
	case ACONFIGURATION_ORIENTATION_LAND:
		appInfo.setDouble(std::max(widthPixels, heightPixels) / density, "width");
		appInfo.setDouble(std::min(widthPixels, heightPixels) / density, "height");
		break;
	}

	return appInfo;
}

const Rc<graphic::ViewImpl> &NativeActivity::waitForView() {
	std::unique_lock lock(rootViewMutex);
	if (rootView) {
		lock.unlock();
	} else {
		rootViewVar.wait(lock, [this] {
			return rootViewTmp != nullptr;
		});
		rootView = move(rootViewTmp);
        rootView->setActivity(this);
	}
	return rootView;
}

void NativeActivity::setDeviceToken(StringView str) {
	if (thread) {
		thread->getApplication()->performOnMainThread([app = thread->getApplication(), str = str.str<Interface>()] {
			app->registerDeviceToken(str);
		});
	}
}

void NativeActivity::handleRemoteNotification() {
	if (thread) {
		thread->getApplication()->performOnMainThread([app = thread->getApplication()] {
			Application::onRemoteNotification(app);
		});
	}
}

void NativeActivity::openUrl(StringView url) {
	rootView->performOnThread([this, url = url.str<Interface>()] {
		auto activityClass = activity->env->GetObjectClass(activity->clazz);
		jmethodID openWebURL = activity->env->GetMethodID(activityClass, "openURL", "(Ljava/lang/String;)V");
		if (openWebURL) {
			jstring jurl = activity->env->NewStringUTF(url.data());
			activity->env->CallVoidMethod(activity->clazz, openWebURL, jurl);
			activity->env->DeleteLocalRef(jurl);
		}
		activity->env->DeleteLocalRef(activityClass);
	});
}

void checkJniError(JNIEnv* env) {
	if (env->ExceptionCheck()) {
		// Read exception msg
		jthrowable e = env->ExceptionOccurred();
		env->ExceptionClear(); // clears the exception; e seems to remain valid
		jclass clazz = env->GetObjectClass(e);
		jclass classClass = env->GetObjectClass(clazz);
		jmethodID getName = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
		jmethodID getMessage = env->GetMethodID(clazz, "getMessage", "()Ljava/lang/String;");
		jstring message = (jstring) env->CallObjectMethod(e, getMessage);
		jstring exName = (jstring) env->CallObjectMethod(clazz, getName);

		const char *mstr = env->GetStringUTFChars(message, NULL);
		log::vtext("JNI", "[", env->GetStringUTFChars(exName, NULL), "] ", mstr);

		env->ReleaseStringUTFChars(message, mstr);
		env->DeleteLocalRef(message);
		env->DeleteLocalRef(exName);
		env->DeleteLocalRef(classClass);
		env->DeleteLocalRef(clazz);
		env->DeleteLocalRef(e);
	}
}

SP_EXTERN_C JNIEXPORT
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
	stappler::log::format("NativeActivity", "Creating: %p %s %s", activity, activity->internalDataPath, activity->externalDataPath);

	NativeActivity *a = new NativeActivity();
	a->init(activity);
}

SP_EXTERN_C JNIEXPORT
void Java_org_stappler_xenolith_appsupport_AppSupportActivity_setDeviceToken(JNIEnv *env, jobject thiz, jlong nativePointer, jstring token) {
	if (nativePointer) {
		auto a = (NativeActivity *)nativePointer;
		a->setDeviceToken(a->activity->env->GetStringUTFChars(token, nullptr));
	}
}

}

namespace stappler::xenolith::platform::graphic {

Rc<gl::View> createView(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info) {
	return Rc<ViewImpl>::create(loop, dev, move(info));
}

}

#endif
