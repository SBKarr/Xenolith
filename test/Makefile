# Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

STAPPLER_ROOT ?= ../../stappler

LOCAL_OUTDIR := stappler-build
LOCAL_EXECUTABLE := testapp

LOCAL_TOOLKIT_EXTERNAL := $(abspath ../xenolith/xenolith.mk)

LOCAL_ROOT = .

LOCAL_SRCS_DIRS :=  src tests
LOCAL_SRCS_OBJS := shaders/XLAppShaders.cpp ../resources/fonts/XLResourceFontSource.cpp

LOCAL_INCLUDES_DIRS := src tests
LOCAL_INCLUDES_OBJS := shaders ../resources/fonts

LOCAL_SHADERS := $(wildcard $(LOCAL_ROOT)/shaders/glsl/*)

include $(STAPPLER_ROOT)/make/universal.mk

$(abspath $(LOCAL_ROOT)/shaders/XLAppShaders.cpp) : $(subst /glsl/,/compiled/shader_,$(LOCAL_SHADERS))

$(LOCAL_ROOT)/shaders/compiled/shader_% : $(LOCAL_ROOT)/shaders/glsl/%
	@$(GLOBAL_MKDIR) $(LOCAL_ROOT)/shaders/compiled
	${GLSLC} -V -o $(LOCAL_ROOT)/shaders/compiled/shader_$* $(LOCAL_ROOT)/shaders/glsl/$* --vn $(subst .,_,$*)

ifdef STAPPLER_TARGET

local-shaders-clean:
	@$(GLOBAL_RM) $(subst /glsl/,/compiled/shader_,$(LOCAL_SHADERS))

endif

host-install: prepare-linux
host-debug: prepare-linux
host-release: prepare-linux

clean: local-shaders-clean

.PHONY: prepare-linux local-shaders-clean
