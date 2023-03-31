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

#include "XLPlatformAndroid.h"

#if ANDROID

namespace stappler::xenolith::platform::graphic {

static int flag_SYSTEM_UI_FLAG_LAYOUT_STABLE;
static int flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
static int flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
static int flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION;
static int flag_SYSTEM_UI_FLAG_FULLSCREEN;
static int flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
static int flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
static int flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;

static int flag_FLAG_TRANSLUCENT_STATUS;
static int flag_FLAG_TRANSLUCENT_NAVIGATION;
static int flag_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS;
static int flag_FLAG_FULLSCREEN;
static int flag_FLAG_LAYOUT_INSET_DECOR;
static int flag_FLAG_LAYOUT_IN_SCREEN;

ViewImpl::ViewImpl() { }

ViewImpl::~ViewImpl() { }

bool ViewImpl::init(gl::Loop &loop, gl::Device &dev, gl::ViewInfo &&info) {
	auto &data = loop.getApplication()->getData();
	info.density = data.density;
	info.rect.width = data.screenSize.width * data.density;
	info.rect.height = data.screenSize.height * data.density;
	info.frameInterval = 1'000'000 / 60;

	if (!View::init(static_cast<vk::Loop &>(loop), static_cast<vk::Device &>(dev), move(info))) {
		return false;
	}

	_options.presentImmediate = true;
	_options.acquireImageImmediately = true;
	_options.renderOnDemand = true;

	return true;
}

void ViewImpl::run() {
	((NativeActivity *)_loop->getApplication()->getNativeHandle())->setView(this);
}

void ViewImpl::threadInit() {
	_started = true;
	View::threadInit();
}

void ViewImpl::mapWindow() {
	View::mapWindow();
}

void ViewImpl::threadDispose() {
	View::threadDispose();
	if (_nativeWindow) {
		ANativeWindow_release(_nativeWindow);
		_nativeWindow = nullptr;
	}
	_surface = nullptr;
	_started = false;
}

bool ViewImpl::worker() {
	return false;
}

void ViewImpl::update(bool displayLink) {
	if (displayLink) {
        if (_initImage) {
            presentImmediate(move(_initImage), nullptr);
            _initImage = nullptr;
            View::update(false);
        	//scheduleNextImage(_frameInterval, false);
            return;
        }

		/*while (_scheduledPresent.empty()) {
			View::update(false);
		}*/
		View::update(true);
	} else {
		View::update(displayLink);
	}
}

void ViewImpl::wakeup() {
	((NativeActivity *)_loop->getApplication()->getNativeHandle())->wakeup();
}

void ViewImpl::updateTextCursor(uint32_t pos, uint32_t len) {

}

void ViewImpl::updateTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::runTextInput(WideStringView str, uint32_t pos, uint32_t len, TextInputType) {

}

void ViewImpl::cancelTextInput() {

}

void ViewImpl::runWithWindow(ANativeWindow *window) {
	auto instance = _instance.cast<vk::Instance>();

	VkSurfaceKHR targetSurface;
	VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.pNext = nullptr;
	surfaceCreateInfo.flags = 0;
	surfaceCreateInfo.window = window;

	_nativeWindow = window;
	_constraints.extent = Extent2(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

	if (instance->vkCreateAndroidSurfaceKHR(instance->getInstance(), &surfaceCreateInfo, nullptr, &targetSurface) != VK_SUCCESS) {
		log::text("ViewImpl", "fail to create surface");
		return;
	}

	_surface = Rc<vk::Surface>::create(instance, targetSurface);
	ANativeWindow_acquire(_nativeWindow);

	auto info = View::getSurfaceOptions();
	if ((info.currentTransform & gl::SurfaceTransformFlags::Rotate90) != gl::SurfaceTransformFlags::None ||
		(info.currentTransform & gl::SurfaceTransformFlags::Rotate270) != gl::SurfaceTransformFlags::None) {
		_identityExtent = Extent2(info.currentExtent.height, info.currentExtent.width);
	} else {
		_identityExtent = info.currentExtent;
	}

	if (!_started) {
		_options.followDisplayLink = true;
		threadInit();
		_options.followDisplayLink = false;
	} else {
		initWindow();
	}
}

void ViewImpl::initWindow() {
	auto info = getSurfaceOptions();
	auto cfg = _selectConfig(info);

	createSwapchain(info, move(cfg), cfg.presentMode);

	if (_initImage && !_options.followDisplayLink) {
		presentImmediate(move(_initImage), nullptr);
		_initImage = nullptr;
	}

	mapWindow();
}

void ViewImpl::stopWindow() {
	_surface = nullptr;

	_swapchain->deprecate(false);
	recreateSwapchain(gl::PresentMode::Unsupported);

	std::unique_lock lock(_windowMutex);
	_loop->performOnGlThread([this] {
		std::unique_lock lock(_windowMutex);
		_loop->waitIdle();
		_windowCond.notify_all();
	});

	_windowCond.wait(lock);

	clearImages();

	for (auto &it : _scheduledPresent) {
		invalidateSwapchainImage(it);
	}

	_scheduledPresent.clear();

	if (_swapchain) {
		_swapchain = nullptr;
	}
	_surface = nullptr;

	if (_nativeWindow) {
		ANativeWindow_release(_nativeWindow);
		_nativeWindow = nullptr;
	}

}

void ViewImpl::setContentPadding(const Padding &padding) {
	_constraints.contentPadding = padding;
	setReadyForNextFrame();
}

void ViewImpl::setActivity(NativeActivity *activity) {
	_activity = activity;

	auto env = _activity->activity->env;

	jclass activityClass = env->FindClass("android/app/NativeActivity");
	jclass windowClass = env->FindClass("android/view/Window");
	jclass viewClass = env->FindClass("android/view/View");
	jclass layoutClass = env->FindClass("android/view/WindowManager$LayoutParams");
	jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
	jmethodID clearFlags = env->GetMethodID(windowClass, "clearFlags", "(I)V");
	jmethodID addFlags = env->GetMethodID(windowClass, "addFlags", "(I)V");

	jobject windowObj = env->CallObjectMethod(_activity->activity->clazz, getWindow);

	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_STABLE = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_STABLE", "I");
	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", "I");
	jfieldID id_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", "I");
	jfieldID id_SYSTEM_UI_FLAG_HIDE_NAVIGATION = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I");
	jfieldID id_SYSTEM_UI_FLAG_FULLSCREEN = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I");
	jfieldID id_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I");
	jfieldID id_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR", "I");
	jfieldID id_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LIGHT_STATUS_BAR", "I");

	flag_SYSTEM_UI_FLAG_LAYOUT_STABLE = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_STABLE);
	flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
	flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
	flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_HIDE_NAVIGATION);
	flag_SYSTEM_UI_FLAG_FULLSCREEN = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_FULLSCREEN);
	flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
	flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
	flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);

	jfieldID id_FLAG_TRANSLUCENT_STATUS = env->GetStaticFieldID(layoutClass, "FLAG_TRANSLUCENT_STATUS", "I");
	jfieldID id_FLAG_TRANSLUCENT_NAVIGATION = env->GetStaticFieldID(layoutClass, "FLAG_TRANSLUCENT_NAVIGATION", "I");
	jfieldID id_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS = env->GetStaticFieldID(layoutClass, "FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS", "I");
	jfieldID id_FLAG_FULLSCREEN = env->GetStaticFieldID(layoutClass, "FLAG_FULLSCREEN", "I");
	jfieldID id_FLAG_LAYOUT_INSET_DECOR = env->GetStaticFieldID(layoutClass, "FLAG_LAYOUT_INSET_DECOR", "I");
	jfieldID id_FLAG_LAYOUT_IN_SCREEN = env->GetStaticFieldID(layoutClass, "FLAG_LAYOUT_IN_SCREEN", "I");

	flag_FLAG_TRANSLUCENT_STATUS = env->GetStaticIntField(layoutClass, id_FLAG_TRANSLUCENT_STATUS);
	flag_FLAG_TRANSLUCENT_NAVIGATION = env->GetStaticIntField(layoutClass, id_FLAG_TRANSLUCENT_NAVIGATION);
	flag_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS = env->GetStaticIntField(layoutClass, id_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
	flag_FLAG_FULLSCREEN = env->GetStaticIntField(layoutClass, id_FLAG_FULLSCREEN);
	flag_FLAG_LAYOUT_INSET_DECOR = env->GetStaticIntField(layoutClass, id_FLAG_LAYOUT_INSET_DECOR);
	flag_FLAG_LAYOUT_IN_SCREEN = env->GetStaticIntField(layoutClass, id_FLAG_LAYOUT_IN_SCREEN);

	env->CallVoidMethod(windowObj, clearFlags, flag_FLAG_TRANSLUCENT_NAVIGATION | flag_FLAG_TRANSLUCENT_STATUS);
	env->CallVoidMethod(windowObj, addFlags, flag_FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS
		| flag_FLAG_LAYOUT_INSET_DECOR | flag_FLAG_LAYOUT_IN_SCREEN);
}

bool ViewImpl::pollInput(bool frameReady) {
	if (!_nativeWindow) {
		return false;
	}

	return true;
}

gl::SurfaceInfo ViewImpl::getSurfaceOptions() const {
	auto info = View::getSurfaceOptions();
	info.currentExtent = _identityExtent;
	return info;
}

void ViewImpl::setDecorationTone(float value) {
	performOnThread([this, value] {
		doSetDecorationTone(value);
	});
}

void ViewImpl::setDecorationVisible(bool value) {
	performOnThread([this, value] {
		doSetDecorationVisible(value);
	});
}

void ViewImpl::doSetDecorationTone(float value) {
	_decorationTone = value;
	updateDecorations();
}

void ViewImpl::doSetDecorationVisible(bool value) {
	_decorationVisible = value;
	updateDecorations();
}

void ViewImpl::updateDecorations() {
	if (!_activity) {
		return;
	}

	auto env = _activity->activity->env;

	jclass activityClass = env->FindClass("android/app/NativeActivity");
	jclass windowClass = env->FindClass("android/view/Window");
	jclass viewClass = env->FindClass("android/view/View");
	jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
	jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
	jmethodID setSystemUiVisibility = env->GetMethodID(viewClass, "setSystemUiVisibility", "(I)V");
	jmethodID getSystemUiVisibility = env->GetMethodID(viewClass, "getSystemUiVisibility", "()I");
	jmethodID setNavigationBarColor = env->GetMethodID(windowClass, "setNavigationBarColor", "(I)V");
	jmethodID setStatusBarColor = env->GetMethodID(windowClass, "setStatusBarColor", "(I)V");
	jmethodID clearFlags = env->GetMethodID(windowClass, "clearFlags", "(I)V");
	jmethodID addFlags = env->GetMethodID(windowClass, "addFlags", "(I)V");

	jobject windowObj = env->CallObjectMethod(_activity->activity->clazz, getWindow);
	jobject decorViewObj = env->CallObjectMethod(windowObj, getDecorView);

	// Get current immersiveness
	const int currentVisibility = env->CallIntMethod(decorViewObj, getSystemUiVisibility);
	int updatedVisibility = currentVisibility;
	updatedVisibility |= flag_SYSTEM_UI_FLAG_LAYOUT_STABLE;
	if (_decorationVisible) {
		updatedVisibility = updatedVisibility & ~flag_SYSTEM_UI_FLAG_FULLSCREEN;
		env->CallVoidMethod(windowObj, clearFlags, flag_FLAG_FULLSCREEN);
	} else {
		updatedVisibility = updatedVisibility | flag_SYSTEM_UI_FLAG_FULLSCREEN;
		//env->CallVoidMethod(windowObj, addFlags, flag_FLAG_FULLSCREEN);
	}
	if (_decorationTone < 0.5f) {
		env->CallVoidMethod(windowObj, setNavigationBarColor, jint(0xFFFFFFFF));
		env->CallVoidMethod(windowObj, setStatusBarColor, jint(0xFFFFFFFF));
		updatedVisibility = updatedVisibility | flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
		updatedVisibility = updatedVisibility | flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
	} else {
		env->CallVoidMethod(windowObj, setNavigationBarColor, jint(0xFF000000));
		env->CallVoidMethod(windowObj, setStatusBarColor, jint(0xFF000000));
		updatedVisibility = updatedVisibility & ~flag_SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
		updatedVisibility = updatedVisibility & ~flag_SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
	}

	// log::vtext("ViewImpl", "setDecorationVisible ", _decorationVisible);
	if (currentVisibility != updatedVisibility) {
		// log::vtext("ViewImpl", "updatedVisibility ", updatedVisibility);
		env->CallVoidMethod(decorViewObj, setSystemUiVisibility, updatedVisibility);
	}

	doCheckError();

	env->DeleteLocalRef(windowObj);
	env->DeleteLocalRef(decorViewObj);
}

void ViewImpl::doCheckError() {
	if (!_activity) {
		return;
	}

	checkJniError(_activity->activity->env);
}

}

#endif
