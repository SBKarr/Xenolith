# Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

# GLSL -> SpirV compiler (default - glslangValidator from https://github.com/KhronosGroup/glslang/releases/tag/master-tot(
GLSLC ?= glslangValidator

# toolkit names
TOOLKIT_NAME := XENOLITH
TOOLKIT_TITLE := xenolith

# current dir
XENOLITH_MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# toolkit source output
# By default - shared between project in stappler's build dir
XENOLITH_OUTPUT_DIR = $(abspath $(TOOLKIT_OUTPUT)/xenolith)
XENOLITH_OUTPUT_STATIC = $(abspath $(TOOLKIT_OUTPUT)/libxenolith.a)

# linker flags and extra libs (libhyphen.a and libfreetype.a from stappler)
OSTYPE_XENOLITH_LIBS += $(OSTYPE_CLI_LIBS) -l:libfreetype.a -lX11 -lXrandr -lXi -lXinerama -lXcursor -lxcb

# use default stappler LDFLAGS for OS
XENOLITH_LDFLAGS := $(OSTYPE_LDFLAGS)

# precompiled headers list
XENOLITH_PRECOMPILED_HEADERS += \
	$(XENOLITH_MAKEFILE_DIR)/core/XLDefine.h \
	components/common/core/SPCommon.h

# sources, common and layout for stappler
XENOLITH_SRCS_DIRS += \
	components/common \
	components/stellator/db \
	$(XENOLITH_MAKEFILE_DIR)/core \
	$(XENOLITH_MAKEFILE_DIR)/gl \
	$(XENOLITH_MAKEFILE_DIR)/features \
	$(XENOLITH_MAKEFILE_DIR)/nodes \

# extra sources: platrom deps and shaders
XENOLITH_SRCS_OBJS += \
	$(XENOLITH_MAKEFILE_DIR)/platform/XLPlatform.scu.cpp \
	$(XENOLITH_MAKEFILE_DIR)/shaders/XLDefaultShaders.cpp \
	$(XENOLITH_MAKEFILE_DIR)/thirdparty/thirdparty.c

# recursive includes
XENOLITH_INCLUDES_DIRS += \
	components/common \
	components/stellator/db \
	$(XENOLITH_MAKEFILE_DIR) \

# non-recursive includes
XENOLITH_INCLUDES_OBJS += \
	$(XENOLITH_MAKEFILE_DIR)/shaders \
	$(OSTYPE_INCLUDE) \
	components/thirdparty

# shaders
XENOLITH_SHADERS := $(wildcard $(XENOLITH_MAKEFILE_DIR)/shaders/glsl/*)
XENOLITH_SHADERS_COMPILED := $(subst /glsl/,/compiled/shader_,$(XENOLITH_SHADERS))

XENOLITH_FLAGS :=

# cleanable output
TOOLKIT_CLEARABLE_OUTPUT := $(XENOLITH_OUTPUT_DIR) $(XENOLITH_OUTPUT_STATIC) $(XENOLITH_SHADERS_COMPILED)

#
# OS extras
#

# MacOS
ifeq ($(UNAME),Darwin)

ifndef LOCAL_MAIN
XENOLITH_SRCS_DIRS += $(XENOLITH_MAKEFILE_DIR)/platform/mac_main
endif

# Windows - Cygwin
else ifeq ($(UNAME),Cygwin)

ifndef LOCAL_MAIN
XENOLITH_SRCS_DIRS += $(XENOLITH_MAKEFILE_DIR)/platform/win32_main
endif

XENOLITH_FLAGS += -I$(shell cygpath -u $(VULKAN_SDK))/Include

# Windows - Msys
else ifeq ($(UNAME),Msys)

ifndef LOCAL_MAIN
XENOLITH_SRCS_DIRS += $(XENOLITH_MAKEFILE_DIR)/platform/win32_main
endif

XENOLITH_FLAGS += -I$(shell cygpath -u $(VULKAN_SDK))/Include

# Linux
else

ifndef LOCAL_MAIN
XENOLITH_SRCS_DIRS += $(XENOLITH_MAKEFILE_DIR)/platform/linux_main
endif

endif

#
# Compilation
#

# Generate source compilation rules
include $(GLOBAL_ROOT)/make/utils/toolkit.mk

# Shader dependencies
$(XENOLITH_MAKEFILE_DIR)/shaders/XLDefaultShaders.cpp : $(subst /glsl/,/compiled/shader_,$(XENOLITH_SHADERS)) 

$(XENOLITH_MAKEFILE_DIR)/shaders/compiled/shader_% : $(XENOLITH_MAKEFILE_DIR)/shaders/glsl/%
	@$(GLOBAL_MKDIR) $(XENOLITH_MAKEFILE_DIR)/shaders/compiled
	${GLSLC} -V -o $(XENOLITH_MAKEFILE_DIR)/shaders/compiled/shader_$* $(XENOLITH_MAKEFILE_DIR)/shaders/glsl/$* --vn $(subst .,_,$*)

# Static library
$(XENOLITH_OUTPUT_STATIC) : $(XENOLITH_H_GCH) $(XENOLITH_GCH) $(XENOLITH_OBJS)
	$(GLOBAL_QUIET_LINK) $(GLOBAL_AR) $(XENOLITH_OUTPUT_STATIC) $(XENOLITH_OBJS)

libxenolith: $(XENOLITH_OUTPUT_STATIC)

.PHONY: libxenolith
