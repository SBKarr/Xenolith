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

ifdef OS_MAKE
MAKE := $(OS_MAKE)
endif

.DEFAULT_GOAL := all

ifndef STAPPLER_TARGET

all:
	@$(MAKE) STAPPLER_TARGET=host all

clean:
	@$(MAKE) STAPPLER_TARGET=host clean

release:
	@$(MAKE) STAPPLER_TARGET=host RELEASE=1

release-clean:
	@$(MAKE) STAPPLER_TARGET=host RELEASE=1 clean

.PHONY: all clean release release-clean

else

THIS_FILE := $(lastword $(MAKEFILE_LIST))

XENOLITH_MAKE_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
XENOLITH_MAKE_ROOT := $(dir $(XENOLITH_MAKE_FILE))
XENOLITH_MAKE_ROOT := $(XENOLITH_MAKE_ROOT:/=)

GLOBAL_ROOT := ../libstappler
LOCAL_TOOLKIT := $(abspath xenolith.mk)

include $(GLOBAL_ROOT)/make/utils/compiler.mk
include $(GLOBAL_ROOT)/make/local.mk

all: libxenolith

.PHONY: all

endif