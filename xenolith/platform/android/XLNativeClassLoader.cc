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

NativeClassLoader::NativeClassLoader() { }

NativeClassLoader::~NativeClassLoader() { }

bool NativeClassLoader::init(ANativeActivity *activity) {
	auto activityClass = activity->env->GetObjectClass(activity->clazz);
	auto classClass = activity->env->GetObjectClass(activityClass);
	auto getClassLoaderMethod = activity->env->GetMethodID(classClass,
			"getClassLoader", "()Ljava/lang/ClassLoader;");

	auto path = stappler::filesystem::platform::Android_getApkPath();
	auto codeCachePath = getCodeCachePath(activity->env, activity->clazz, activityClass);
	auto paths = getNativePaths(activity->env, activity->clazz, activityClass);

	apkPath = activity->env->GetStringUTFChars(paths.apkPath, NULL);
	nativeLibraryDir = activity->env->GetStringUTFChars(paths.nativeLibraryDir, NULL);

	filesystem::ftw(nativeLibraryDir, [] (StringView path, bool isFile) {
		if (isFile) {
			log::text("NativeClassLoader", path);
		}
	});

	auto classLoader = activity->env->CallObjectMethod(activityClass, getClassLoaderMethod);

	activity->env->DeleteLocalRef(activityClass);
	activity->env->DeleteLocalRef(classClass);

	if (!codeCachePath || !paths.apkPath) {
		checkJniError(activity->env);
		return false;
	}

	if (classLoader) {
		activityClassLoader = activity->env->NewGlobalRef(classLoader);
		activity->env->DeleteLocalRef(classLoader);

		auto classLoaderClass = activity->env->GetObjectClass(activityClassLoader);
		activityClassLoaderClass = (jclass)activity->env->NewGlobalRef(classLoaderClass);
		activity->env->DeleteLocalRef(classLoaderClass);

		auto className = getClassName(activity->env, activityClassLoaderClass);
		const char *mstr = activity->env->GetStringUTFChars(className, NULL);

		log::vtext("JNI", "Activity: ClassLoader: ", mstr);
		if (StringView(mstr) == StringView("java.lang.BootClassLoader")) {
			// acquire new dex class loader
			auto dexClassLoaderClass = activity->env->FindClass("dalvik/system/DexClassLoader");
			if (!dexClassLoaderClass) {
				checkJniError(activity->env);
				return false;
			}

			jmethodID dexClassLoaderConstructor = activity->env->GetMethodID(dexClassLoaderClass, "<init>",
					"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");

			auto dexLoader = activity->env->NewObject(dexClassLoaderClass, dexClassLoaderConstructor,
					paths.apkPath, codeCachePath, paths.nativeLibraryDir, activityClassLoader);
			if (dexLoader) {
				apkClassLoader = activity->env->NewGlobalRef(dexLoader);
				activity->env->DeleteLocalRef(dexLoader);

				apkClassLoaderClass = (jclass)activity->env->NewGlobalRef(dexClassLoaderClass);
				activity->env->DeleteLocalRef(dexClassLoaderClass);

				findClassMethod = activity->env->GetMethodID(apkClassLoaderClass, "loadClass", "(Ljava/lang/String;Z)Ljava/lang/Class;");
			} else {
				checkJniError(activity->env);
				return false;
			}
		} else {
			apkClassLoader = activity->env->NewGlobalRef(activityClassLoader);
			apkClassLoaderClass = (jclass)activity->env->NewGlobalRef(activityClassLoaderClass);
			findClassMethod = activity->env->GetMethodID(activityClassLoaderClass, "loadClass", "(Ljava/lang/String;Z)Ljava/lang/Class;");
		}
	}

	checkJniError(activity->env);
	return true;
}

void NativeClassLoader::finalize(JNIEnv *env) {
	if (activityClassLoader) {
		env->DeleteGlobalRef(activityClassLoader);
		activityClassLoader = nullptr;
	}
	if (activityClassLoaderClass) {
		env->DeleteGlobalRef(activityClassLoaderClass);
		activityClassLoaderClass = nullptr;
	}
	if (apkClassLoader) {
		env->DeleteGlobalRef(apkClassLoader);
		apkClassLoader = nullptr;
	}
	if (apkClassLoaderClass) {
		env->DeleteGlobalRef(apkClassLoaderClass);
		apkClassLoaderClass = nullptr;
	}
	findClassMethod = nullptr;
}

jclass NativeClassLoader::findClass(JNIEnv *env, StringView data) {
	auto tmp = env->NewStringUTF(data.data());
	auto ret = findClass(env, tmp);
	env->DeleteLocalRef(tmp);
	return ret;
}

jclass NativeClassLoader::findClass(JNIEnv *env, jstring name) {
	auto ret = (jclass)env->CallObjectMethod(apkClassLoader, findClassMethod, name, jboolean(1));
	checkJniError(env);
	return ret;
}

jstring NativeClassLoader::getClassName(JNIEnv *env, jclass cl) {
	jclass classClass = env->GetObjectClass(cl);
	jmethodID getName = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
	jstring name = (jstring) env->CallObjectMethod(cl, getName);
	env->DeleteLocalRef(classClass);
	return name;
}

NativeClassLoader::NativePaths NativeClassLoader::getNativePaths(JNIEnv *env, jobject context, jclass cl) {
	NativePaths ret;
	bool hasClass = (cl != nullptr);
	if (!cl) {
		cl = env->GetObjectClass(context);
	}

	auto getPackageNameMethod = env->GetMethodID(cl, "getPackageName", "()Ljava/lang/String;");
	auto getPackageManagerMethod = env->GetMethodID(cl, "getPackageManager", "()Landroid/content/pm/PackageManager;");

	auto packageName = (jstring)env->CallObjectMethod(context, getPackageNameMethod);
	auto packageManager = env->CallObjectMethod(context, getPackageManagerMethod);

	if (packageName && packageManager) {
		auto packageManagerClass = env->GetObjectClass(packageManager);
		//auto packageManagerClass = env->FindClass("android/content/pm/PackageManager");
		auto getApplicationInfoMethod = env->GetMethodID(packageManagerClass, "getApplicationInfo",
				"(Ljava/lang/String;I)Landroid/content/pm/ApplicationInfo;");

		auto applicationInfo = env->CallObjectMethod(packageManager, getApplicationInfoMethod, packageName, 0);

		if (applicationInfo) {
			auto applicationInfoClass = env->GetObjectClass(applicationInfo);
			auto publicSourceDirField = env->GetFieldID(applicationInfoClass, "publicSourceDir", "Ljava/lang/String;");
			auto nativeLibraryDirField = env->GetFieldID(applicationInfoClass, "nativeLibraryDir", "Ljava/lang/String;");

			ret.apkPath = (jstring)env->GetObjectField(applicationInfo, publicSourceDirField);
			ret.nativeLibraryDir = (jstring)env->GetObjectField(applicationInfo, nativeLibraryDirField);

			env->DeleteLocalRef(applicationInfoClass);
			env->DeleteLocalRef(applicationInfo);
		}

		if (packageManagerClass) { env->DeleteLocalRef(packageManagerClass); }
	}

	checkJniError(env);

	if (packageName) { env->DeleteLocalRef(packageName); }
	if (packageManager) { env->DeleteLocalRef(packageManager); }

	if (!hasClass) {
		env->DeleteLocalRef(cl);
	}

	return ret;
}

jstring NativeClassLoader::getCodeCachePath(JNIEnv *env, jobject context, jclass cl) {
	jstring codeCachePath = nullptr;
	bool hasClass = (cl != nullptr);
	if (!cl) {
		cl = env->GetObjectClass(context);
	}

	auto getCodeCacheDirMethod = env->GetMethodID(cl, "getCodeCacheDir", "()Ljava/io/File;");

	auto codeCacheDir = env->CallObjectMethod(context, getCodeCacheDirMethod);
	auto fileClass = env->GetObjectClass(codeCacheDir);
	auto getAbsolutePathMethod = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");

	codeCachePath = (jstring)env->CallObjectMethod(codeCacheDir, getAbsolutePathMethod);

	if (codeCacheDir) { env->DeleteLocalRef(codeCacheDir); }
	if (fileClass) { env->DeleteLocalRef(fileClass); }

	if (!hasClass) {
		env->DeleteLocalRef(cl);
	}

	return codeCachePath;
}

}

#endif
