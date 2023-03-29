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

namespace stappler::xenolith::platform {

struct NetworkConnectivity {
	JNIEnv *env = nullptr;
	jobject thiz = nullptr;

	void (*callback) (JNIEnv *, jobject, void *) = nullptr;
	void *listener = nullptr;
};

static jlong NetworkConnectivity_nativeOnCreated(JNIEnv *env, jobject thiz) {
	__android_log_write(ANDROID_LOG_INFO, "NetworkConnectivity", "nativeOnCreated");
	auto obj = new NetworkConnectivity{env, thiz, nullptr, nullptr};
	return (jlong)obj;
}

static void NetworkConnectivity_nativeOnFinalized(JNIEnv *env, jobject thiz, jlong nativePointer) {
	__android_log_write(ANDROID_LOG_INFO, "NetworkConnectivity", "nativeOnFinalized");
	if (nativePointer) {
		delete (NetworkConnectivity *)nativePointer;
	}
}

static void NetworkConnectivity_nativeOnAvailable(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->env = env;
	native->thiz = thiz;
	__android_log_write(ANDROID_LOG_INFO, "NetworkConnectivity", "nativeOnAvailable");
}

static void NetworkConnectivity_nativeOnLost(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->env = env;
	native->thiz = thiz;
	__android_log_write(ANDROID_LOG_INFO, "NetworkConnectivity", "nativeOnLost");
}

static void NetworkConnectivity_nativeOnCapabilitiesChanged(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->env = env;
	native->thiz = thiz;
	__android_log_write(ANDROID_LOG_INFO, "NetworkConnectivity", "nativeOnCapabilitiesChanged");
}

static void NetworkConnectivity_nativeOnLinkPropertiesChanged(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->env = env;
	native->thiz = thiz;
	__android_log_write(ANDROID_LOG_INFO, "NetworkConnectivity", "nativeOnLinkPropertiesChanged");
}

static JNINativeMethod methods[] = {
	{"nativeOnCreated","()J", (void *)&NetworkConnectivity_nativeOnCreated},
	{"nativeOnFinalized","(J)V", (void *)&NetworkConnectivity_nativeOnFinalized},
	{"nativeOnAvailable", "(J)V", (void *)&NetworkConnectivity_nativeOnAvailable},
	{"nativeOnLost", "(J)V", (void *)&NetworkConnectivity_nativeOnLost},
	{"nativeOnCapabilitiesChanged", "(J)V", (void *)&NetworkConnectivity_nativeOnCapabilitiesChanged},
	{"nativeOnLinkPropertiesChanged", "(J)V", (void *)&NetworkConnectivity_nativeOnLinkPropertiesChanged},
};

void linkNetworkConnectivityClass(JNIEnv *env, jclass cl) {
	env->RegisterNatives(cl, methods, 6);
}

}

#endif
