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

static void NetworkConnectivity_nativeOnCreated(JNIEnv *env, jobject thiz, jlong nativePointer, int flags) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->handleCreated(flags);
}

static void NetworkConnectivity_nativeOnFinalized(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->handleFinalized();
}

static void NetworkConnectivity_nativeOnAvailable(JNIEnv *env, jobject thiz, jlong nativePointer, int flags) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->handleAvailable(flags);
}

static void NetworkConnectivity_nativeOnLost(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->handleLost();
}

static void NetworkConnectivity_nativeOnCapabilitiesChanged(JNIEnv *env, jobject thiz, jlong nativePointer, int flags) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->handleCapabilitiesChanged(flags);
}

static void NetworkConnectivity_nativeOnLinkPropertiesChanged(JNIEnv *env, jobject thiz, jlong nativePointer) {
	auto native = (NetworkConnectivity *)nativePointer;
	native->handleLinkPropertiesChanged();
}

static JNINativeMethod methods[] = {
	{"nativeOnCreated","(JI)V", (void *)&NetworkConnectivity_nativeOnCreated},
	{"nativeOnFinalized","(J)V", (void *)&NetworkConnectivity_nativeOnFinalized},
	{"nativeOnAvailable", "(JI)V", (void *)&NetworkConnectivity_nativeOnAvailable},
	{"nativeOnLost", "(J)V", (void *)&NetworkConnectivity_nativeOnLost},
	{"nativeOnCapabilitiesChanged", "(JI)V", (void *)&NetworkConnectivity_nativeOnCapabilitiesChanged},
	{"nativeOnLinkPropertiesChanged", "(J)V", (void *)&NetworkConnectivity_nativeOnLinkPropertiesChanged},
};

void linkNetworkConnectivityClass(JNIEnv *env, jclass cl) {
	env->RegisterNatives(cl, methods, 6);
}

bool NetworkConnectivity::init(JNIEnv *env, NativeClassLoader *classLoader, jobject context, Function<void(NetworkCapabilities)> &&cb) {
	jclass networkConnectivityClass = classLoader->findClass(env, "org.stappler.xenolith.appsupport.NetworkConnectivity");
	if (networkConnectivityClass) {
		env->RegisterNatives(networkConnectivityClass, methods, 6);
		jmethodID networkConnectivityCreate = env->GetStaticMethodID(networkConnectivityClass, "create",
				"(Landroid/content/Context;J)Lorg/stappler/xenolith/appsupport/NetworkConnectivity;");
		if (networkConnectivityCreate) {
			auto conn = env->CallStaticObjectMethod(networkConnectivityClass, networkConnectivityCreate, context, jlong(this));
			if (conn) {
				thiz = env->NewGlobalRef(conn);
				clazz = (jclass)env->NewGlobalRef(networkConnectivityClass);
				env->DeleteLocalRef(conn);
				env->DeleteLocalRef(networkConnectivityClass);
				callback = move(cb);
				if (callback) {
					callback(capabilities);
				}
				return true;
			}
		} else {
			checkJniError(env);
		}
		env->DeleteLocalRef(networkConnectivityClass);
	}
	return false;
}

void NetworkConnectivity::finalize(JNIEnv *env) {
	if (thiz && clazz) {
		jmethodID networkConnectivityFinalize = env->GetMethodID(clazz, "finalize", "()V");
		if (networkConnectivityFinalize) {
			env->CallVoidMethod(thiz, networkConnectivityFinalize);
		}
	}
	if (thiz) {
		env->DeleteGlobalRef(thiz);
		thiz = nullptr;
	}
	if (clazz) {
		env->DeleteGlobalRef(clazz);
		thiz = nullptr;
	}
}

void NetworkConnectivity::handleCreated(int flags) {
	capabilities = NetworkCapabilities(flags);
	if (callback) {
		callback(capabilities);
	}
}

void NetworkConnectivity::handleFinalized() {
	capabilities = NetworkCapabilities::None;
	callback = nullptr;
}

void NetworkConnectivity::handleAvailable(int flags) {
	capabilities = NetworkCapabilities(flags);
	if (callback) {
		callback(capabilities);
	}
}

void NetworkConnectivity::handleLost() {
	capabilities = NetworkCapabilities::None;
	if (callback) {
		callback(capabilities);
	}
}

void NetworkConnectivity::handleCapabilitiesChanged(int flags) {
	capabilities = NetworkCapabilities(flags);
	if (callback) {
		callback(capabilities);
	}
}

void NetworkConnectivity::handleLinkPropertiesChanged() {

}

}

#endif
