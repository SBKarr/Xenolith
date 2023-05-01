STAPPLER_ROOT ?= ../../libstappler

LOCAL_OUTDIR := stappler-build
LOCAL_EXECUTABLE := embedder

LOCAL_TOOLKIT := $(abspath $(STAPPLER_ROOT)/common.mk)
LOCAL_TOOLKIT_OUTPUT := $(realpath .)/$(LOCAL_OUTDIR)

LOCAL_ROOT = .

LOCAL_SRCS_DIRS :=
LOCAL_SRCS_OBJS :=

LOCAL_INCLUDES_DIRS :=
LOCAL_INCLUDES_OBJS :=

LOCAL_MAIN := main.cpp

LOCAL_LIBS =

LOCAL_MODULES ?= \
	common_filesystem \
	common_data \
	common_brotli_lib

LOCAL_FORCE_INSTALL := 1

include $(STAPPLER_ROOT)/make/universal.mk
