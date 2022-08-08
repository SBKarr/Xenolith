STAPPLER_ROOT ?= ../../libstappler

LOCAL_OUTDIR := stappler-build
LOCAL_EXECUTABLE := headergen

LOCAL_TOOLKIT := $(abspath $(STAPPLER_ROOT)/common.mk)

LOCAL_ROOT = .

LOCAL_SRCS_DIRS := src
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS := src
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

LOCAL_LIBS =

LOCAL_MODULES ?= \
	common_filesystem \
	common_network \
	common_bitmap \
	common_vg

#.local_prebuild:
#	@echo test

include $(STAPPLER_ROOT)/make/universal.mk
