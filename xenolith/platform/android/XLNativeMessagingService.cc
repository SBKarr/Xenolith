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

extern "C"
JNIEXPORT jlong JNICALL
Java_org_stappler_xenolith_appsupport_MessagingService_onCreated(JNIEnv *env, jobject thiz) {
	log::text("MessagingService", "onCreated");
	return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_appsupport_MessagingService_onDestroyed(JNIEnv *env, jobject thiz, jlong nativePointer) {
	log::text("MessagingService", "onDestroyed");

}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_appsupport_MessagingService_onRemoteNotification(JNIEnv *env, jobject thiz, jlong nativePointer, jstring header, jstring body) {
	if (auto a = NativeActivity::getInstance()) {
		a->handleRemoteNotification();
	}
	log::text("MessagingService", "onRemoteNotification");
}

extern "C"
JNIEXPORT void JNICALL
Java_org_stappler_xenolith_appsupport_MessagingService_onTokenReceived(JNIEnv *env, jobject thiz, jlong nativePointer, jstring token) {

	log::text("MessagingService", "onTokenReceived");
}

}

#endif
