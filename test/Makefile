# Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
# Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

STAPPLER_ROOT ?= ../libstappler

LOCAL_OUTDIR ?= stappler-build
LOCAL_EXECUTABLE := testapp

LOCAL_TOOLKIT := $(abspath ../xenolith/xenolith.mk)

LOCAL_ROOT = .

LOCAL_SRCS_DIRS :=  src
LOCAL_SRCS_OBJS := shaders/XLAppShaders.cpp

LOCAL_INCLUDES_DIRS := src
LOCAL_INCLUDES_OBJS := shaders ../resources/fonts

LOCAL_SHADERS := $(wildcard $(LOCAL_ROOT)/shaders/glsl/*)

LOCAL_MODULES ?= \
	xenolith_icons \
	xenolith_gui \
	xenolith_storage \
	xenolith_network \
	xenolith_material \
	xenolith_asset

# Android config
LOCAL_ANDROID_MK := proj.android/app/Android.mk # path to root Android.mk
LOCAL_APPLICATION_MK := proj.android/app/Application.mk # path to Application.mk
LOCAL_ANDROID_TARGET := application # target to build
LOCAL_ANDROID_PLATFORM := android-24 # android platform version

include $(STAPPLER_ROOT)/make/universal.mk

prepare-android:
	@mkdir -p proj.android/app/assets
	rsync -ar resources proj.android/app/assets

ifdef STAPPLER_TARGET

local-shaders-clean:
	@$(GLOBAL_RM) $(subst /glsl/,/compiled/shader_,$(LOCAL_SHADERS))

ifdef MACOS_ARCH
$(BUILD_EXECUTABLE): xenolith-install-loader
endif

android-export: prepare-android

endif

.PHONY: prepare-linux prepare-android
