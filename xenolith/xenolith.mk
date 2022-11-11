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

# VULKAN_SDK_PREFIX ?= ~/VulkanSDK/<version>/<OS>

# GLSL -> SpirV compiler (default - glslangValidator from https://github.com/KhronosGroup/glslang/releases/tag/master-tot)
ifdef VULKAN_SDK_PREFIX
GLSLC ?= $(VULKAN_SDK_PREFIX)/bin/glslangValidator
SPIRV_LINK ?= $(VULKAN_SDK_PREFIX)/bin/spirv-link
else
GLSLC ?= glslangValidator
SPIRV_LINK ?= spirv-link
endif

# toolkit names
TOOLKIT_NAME := XENOLITH
TOOLKIT_TITLE := xenolith

# current dir
XENOLITH_MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# toolkit source output
# By default - shared between project in stappler's build dir
XENOLITH_OUTPUT_DIR = $(abspath $(TOOLKIT_OUTPUT)/xenolith)
XENOLITH_OUTPUT_STATIC = $(abspath $(TOOLKIT_OUTPUT)/libxenolith.a)

# linker flags and extra libs
OSTYPE_XENOLITH_LIBS += $(OSTYPE_CLI_LIBS) -l:libfreetype.a

# use default stappler LDFLAGS for OS
XENOLITH_LDFLAGS := $(OSTYPE_LDFLAGS)

# precompiled headers list
XENOLITH_PRECOMPILED_HEADERS += \
	$(XENOLITH_MAKEFILE_DIR)/core/XLDefine.h \
	common/core/SPCommon.h

# sources, common and layout for stappler
XENOLITH_SRCS_DIRS += \
	common \
	thirdparty \
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
	common \
	$(XENOLITH_MAKEFILE_DIR) \

# non-recursive includes
XENOLITH_INCLUDES_OBJS += \
	$(XENOLITH_MAKEFILE_DIR)/shaders \
	$(OSTYPE_INCLUDE) \
	thirdparty

ifdef VULKAN_SDK_PREFIX
XENOLITH_INCLUDES_OBJS += $(VULKAN_SDK_PREFIX)/include
endif

# shaders
XENOLITH_EMBEDDER_DIR = $(realpath $(XENOLITH_MAKEFILE_DIR)/../utils/embedder)
XENOLITH_EMBEDDER = $(XENOLITH_EMBEDDER_DIR)/stappler-build/host/embedder -f
XENOLITH_SHADERS := $(wildcard $(XENOLITH_MAKEFILE_DIR)/shaders/glsl/*/*)
XENOLITH_SHADERS_COMPILED := $(subst /glsl/,/compiled/,$(XENOLITH_SHADERS))
XENOLITH_SHADERS_LINKED := $(subst /glsl/,/linked/,$(wildcard $(XENOLITH_MAKEFILE_DIR)/shaders/glsl/*))
XENOLITH_SHADERS_EMBEDDED := $(subst /linked/,/embedded/,$(XENOLITH_SHADERS_LINKED))

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

VULKAN_LOADER_PATH = $(realpath $(VULKAN_SDK_PREFIX)/lib/libvulkan.1.dylib)
VULKAN_MOLTENVK_PATH = $(realpath $(VULKAN_SDK_PREFIX)/../MoltenVK/dylib/macOS/libMoltenVK.dylib)
VULKAN_MOLTENVK_ICD_PATH = $(realpath $(VULKAN_SDK_PREFIX)/../MoltenVK/dylib/macOS/MoltenVK_icd.json)
VULKAN_LAYERS_PATH = $(realpath $(VULKAN_SDK_PREFIX)/share/vulkan/explicit_layer.d)
VULKAN_LIBDIR = $(dir $(BUILD_EXECUTABLE))vulkan

$(VULKAN_LIBDIR)/libvulkan.dylib: $(VULKAN_LOADER_PATH)
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)
	cp $(VULKAN_LOADER_PATH) $(VULKAN_LIBDIR)/libvulkan.dylib

$(VULKAN_LIBDIR)/icd.d/libMoltenVK.dylib: $(VULKAN_MOLTENVK_PATH)
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/icd.d
	cp $(VULKAN_MOLTENVK_PATH) $(VULKAN_LIBDIR)/icd.d/libMoltenVK.dylib

$(VULKAN_LIBDIR)/icd.d/MoltenVK_icd.json: $(VULKAN_MOLTENVK_ICD_PATH)
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/icd.d
	cp $(VULKAN_MOLTENVK_ICD_PATH) $(VULKAN_LIBDIR)/icd.d/MoltenVK_icd.json

$(VULKAN_LIBDIR)/explicit_layer.d/%.json: $(VULKAN_LAYERS_PATH)/%.json
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/explicit_layer.d
	sed 's/..\/..\/..\/lib\/libVkLayer/..\/lib\/libVkLayer/g' $(VULKAN_LAYERS_PATH)/$*.json > $(VULKAN_LIBDIR)/explicit_layer.d/$*.json

$(VULKAN_LIBDIR)/lib/%.dylib: $(VULKAN_SDK_PREFIX)/lib/%.dylib
	@$(GLOBAL_MKDIR) $(VULKAN_LIBDIR)/lib
	cp $(VULKAN_SDK_PREFIX)/lib/$*.dylib $(VULKAN_LIBDIR)/lib/$*.dylib

xenolith-install-loader: $(VULKAN_LIBDIR)/libvulkan.dylib $(VULKAN_LIBDIR)/icd.d/libMoltenVK.dylib $(VULKAN_LIBDIR)/icd.d/MoltenVK_icd.json \
	$(subst $(VULKAN_LAYERS_PATH),$(VULKAN_LIBDIR)/explicit_layer.d,$(wildcard $(VULKAN_LAYERS_PATH)/*.json)) \
	$(subst $(VULKAN_SDK_PREFIX),$(VULKAN_LIBDIR),$(wildcard $(VULKAN_SDK_PREFIX)/lib/libVkLayer_*.dylib))

.PHONY: xenolith-install-loader

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

include $(GLOBAL_ROOT)/common-modules.mk
include $(XENOLITH_MAKEFILE_DIR)/xenolith-modules.mk

LOCAL_MODULES += \
	common_data \
	common_threads \
	common_bitmap \
	common_tess \
	common_vg \
	common_zip \
	common_brotli_lib \
	common_filesystem \
	common_backtrace

include $(GLOBAL_ROOT)/make/utils/resolve-modules.mk

# Generate source compilation rules
include $(GLOBAL_ROOT)/make/utils/make-toolkit.mk

# Shader dependencies
$(XENOLITH_MAKEFILE_DIR)/shaders/XLDefaultShaders.cpp : $(XENOLITH_SHADERS_EMBEDDED) 

$(XENOLITH_EMBEDDER) :
	$(MAKE) -C $(XENOLITH_EMBEDDER_DIR) install

$(XENOLITH_MAKEFILE_DIR)/shaders/embedded/% : $(XENOLITH_MAKEFILE_DIR)/shaders/linked/% | $(XENOLITH_EMBEDDER)
	@$(GLOBAL_MKDIR) $(dir $@)
	$(XENOLITH_EMBEDDER) --output $@ --input $< --name $(subst .,_,$*)

$(XENOLITH_MAKEFILE_DIR)/shaders/linked/% : $(XENOLITH_SHADERS_COMPILED)
	@$(GLOBAL_MKDIR) $(dir $@)
	$(SPIRV_LINK) -o $@ $(subst /glsl/,/compiled/,$(wildcard $(subst /linked/,/glsl/,$@)/*))

$(XENOLITH_MAKEFILE_DIR)/shaders/compiled/% : $(XENOLITH_MAKEFILE_DIR)/shaders/glsl/%
	@$(GLOBAL_MKDIR) $(dir $@)
	${GLSLC} -V -o $(XENOLITH_MAKEFILE_DIR)/shaders/compiled/$* $(XENOLITH_MAKEFILE_DIR)/shaders/glsl/$* -e $(notdir $(basename $*)) --sep main

# Static library
$(XENOLITH_OUTPUT_STATIC) : $(XENOLITH_H_GCH) $(XENOLITH_GCH) $(XENOLITH_OBJS)
	$(GLOBAL_QUIET_LINK) $(GLOBAL_AR) $(XENOLITH_OUTPUT_STATIC) $(XENOLITH_OBJS)

libxenolith: $(XENOLITH_OUTPUT_STATIC)

.PHONY: libxenolith
