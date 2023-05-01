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

#ifndef XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROID_H_
#define XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROID_H_

#include "XLPlatform.h"
#include "XLVkView.h"

#if ANDROID

#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_activity.h>

#include <android/log.h>

namespace stappler::xenolith::platform {

struct NativeActivity;

}

namespace stappler::xenolith::platform::graphic {

class ViewImpl : public vk::View {
public:
	ViewImpl();
	virtual ~ViewImpl();

	virtual bool init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info);

	virtual void run() override;
	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;
	virtual void update(bool displayLink) override;

	virtual void wakeup() override;
	virtual void mapWindow() override;

	virtual void updateTextCursor(uint32_t pos, uint32_t len) override;
	virtual void updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) override;
	virtual void cancelTextInput() override;

	void runWithWindow(ANativeWindow *);
	void initWindow();
	void stopWindow();

	void setContentPadding(const Padding &);
	void setActivity(NativeActivity *);

	virtual void setDecorationTone(float) override;
	virtual void setDecorationVisible(bool) override;

protected:
	using vk::View::init;

	virtual bool pollInput(bool frameReady) override;
	virtual gl::SurfaceInfo getSurfaceOptions() const override;

	void doSetDecorationTone(float);
    void doSetDecorationVisible(bool);
	void updateDecorations();
    void doCheckError();

	bool _started = false;
	ANativeWindow *_nativeWindow = nullptr;
	Extent2 _identityExtent;
	NativeActivity *_activity = nullptr;

	float _decorationTone = 0.0f;
	bool _decorationVisible = true;

	std::mutex _windowMutex;
	std::condition_variable _windowCond;
};

}

namespace stappler::xenolith::platform {

enum class WindowFlags : uint32_t {
	NONE = 0,
	FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS = 0x80000000,
};

SP_DEFINE_ENUM_AS_MASK(WindowFlags)

class EngineMainThread : public thread::ThreadInterface<Interface> {
public:
	virtual ~EngineMainThread();

	bool init(Application *, Value &&);

	virtual void threadInit() override;
	virtual void threadDispose() override;
	virtual bool worker() override;

	void waitForRunning();

	const Rc<Application> &getApplication() const { return _application; }

protected:
	Rc<Application> _application;
	Value _args;

	std::atomic<bool> _running = false;
	std::mutex _runningMutex;
	std::condition_variable _runningVar;

	std::thread _thread;
	std::thread::id _threadId;
};

struct NativeClassLoader : Ref {
	struct NativePaths {
		jstring apkPath = nullptr;
		jstring nativeLibraryDir = nullptr;
	};

	jobject activityClassLoader = nullptr;
	jclass activityClassLoaderClass = nullptr;

	jobject apkClassLoader = nullptr;
	jclass apkClassLoaderClass = nullptr;

	jmethodID findClassMethod = nullptr;

	String apkPath;
	String nativeLibraryDir;

	NativeClassLoader();
	~NativeClassLoader();

	bool init(ANativeActivity *activity);
	void finalize(JNIEnv *);

	jclass findClass(JNIEnv *, StringView);
	jclass findClass(JNIEnv *, jstring);
	jstring getClassName(JNIEnv *, jclass);
	NativePaths getNativePaths(JNIEnv *, jobject, jclass = nullptr);
	jstring getCodeCachePath(JNIEnv *, jobject, jclass = nullptr);
};

enum class NetworkCapabilities {
	None,
	NET_CAPABILITY_INTERNET = (1 << 0),
	NET_CAPABILITY_NOT_CONGESTED = (1 << 1),
	NET_CAPABILITY_NOT_METERED = (1 << 2),
	NET_CAPABILITY_NOT_RESTRICTED = (1 << 3),
	NET_CAPABILITY_NOT_ROAMING = (1 << 4),
	NET_CAPABILITY_NOT_SUSPENDED = (1 << 5),
	NET_CAPABILITY_NOT_VPN = (1 << 6),
	NET_CAPABILITY_PRIORITIZE_BANDWIDTH = (1 << 7),
	NET_CAPABILITY_PRIORITIZE_LATENCY = (1 << 8),
	NET_CAPABILITY_TEMPORARILY_NOT_METERED = (1 << 9),
	NET_CAPABILITY_TRUSTED = (1 << 10),
	NET_CAPABILITY_VALIDATED = (1 << 11),
	NET_CAPABILITY_WIFI_P2P = (1 << 12),
};

SP_DEFINE_ENUM_AS_MASK(NetworkCapabilities)

struct NetworkConnectivity : public Ref {
	jclass clazz = nullptr;
	jobject thiz = nullptr;
	NetworkCapabilities capabilities;
	Function<void(NetworkCapabilities)> callback;

	bool init(JNIEnv *, NativeClassLoader *, jobject context, Function<void(NetworkCapabilities)> && = nullptr);
	void finalize(JNIEnv *);

	void handleCreated(int flags);
	void handleFinalized();
	void handleAvailable(int flags);
	void handleLost();
	void handleCapabilitiesChanged(int flags);
	void handleLinkPropertiesChanged();
};

struct NativeBufferFormatSupport {
	bool R8G8B8A8_UNORM = true;
	bool R8G8B8X8_UNORM = true;
	bool R8G8B8_UNORM = true;
	bool R5G6B5_UNORM = true;
	bool R16G16B16A16_FLOAT = true;
	bool R10G10B10A2_UNORM = true;
};

struct NativeActivity {
	struct InputLooperData {
		NativeActivity *activity;
		AInputQueue *queue;
	};

	ANativeActivity *activity = nullptr;
	AConfiguration *config = nullptr;
	ALooper *looper = nullptr;
	Rc<EngineMainThread> thread;
	Rc<NativeClassLoader> classLoader;
	Rc<NetworkConnectivity> networkConnectivity;

	Rc<graphic::ViewImpl> rootViewTmp;
	Rc<graphic::ViewImpl> rootView;
	std::mutex rootViewMutex;
	std::condition_variable rootViewVar;

	int _eventfd = -1;
	int _timerfd = -1;

	Map<AInputQueue *, InputLooperData> _input;

	float _density = 1.0f;
	InputModifier _activeModifiers = InputModifier::None;
	Size2 _windowSize;
	Vec2 _hoverLocation;
	int32_t sdkVersion = 0;

	NativeBufferFormatSupport formatSupport;
	bool isEmulator = false;

	static NativeActivity *getInstance();

	~NativeActivity();
	NativeActivity() = default;

	jclass getClass() const {
		return activity->env->GetObjectClass(activity->clazz);
	}

	jmethodID getMethodID(jclass cl, StringView method, StringView params) const {
		return activity->env->GetMethodID(cl, method.data(), params.data());
	}

	bool init(ANativeActivity *a);

	void wakeup();
	void setView(graphic::ViewImpl *);

	void handleConfigurationChanged();
	void handleContentRectChanged(const ARect *);

	void handleInputQueueCreated(AInputQueue *);
	void handleInputQueueDestroyed(AInputQueue *);
	void handleLowMemory();
	void* handleSaveInstanceState(size_t* outLen);

	void handleNativeWindowCreated(ANativeWindow *);
	void handleNativeWindowDestroyed(ANativeWindow *);
	void handleNativeWindowRedrawNeeded(ANativeWindow *);
	void handleNativeWindowResized(ANativeWindow *);

	void handlePause();
	void handleStart();
	void handleResume();
	void handleStop();

	void handleWindowFocusChanged(int focused);

	int handleLooperEvent(int fd, int events);
	int handleInputEventQueue(int fd, int events, AInputQueue *);
	int handleInputEvent(AInputEvent *);
	int handleKeyEvent(AInputEvent *);
	int handleMotionEvent(AInputEvent *);

	Value getAppInfo(AConfiguration *);

	const Rc<graphic::ViewImpl> &waitForView();

	void setDeviceToken(StringView);
	void handleRemoteNotification();

	void openUrl(StringView url);
};

void checkJniError(JNIEnv *env);

}

#endif

#endif /* XENOLITH_PLATFORM_ANDROID_XLPLATFORMANDROID_H_ */
