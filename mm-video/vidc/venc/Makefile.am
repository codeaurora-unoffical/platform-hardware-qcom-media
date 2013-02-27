AM_CFLAGS = -Wall
AM_CFLAGS += -Wundef
AM_CFLAGS += -Wstrict-prototypes
AM_CFLAGS += -Wno-trigraphs
AM_CFLAGS += -g -O3

AM_CPPFLAGS := -D__alignx\(x\)=__attribute__\(\(__aligned__\(x\)\)\)
AM_CPPFLAGS += -D__align=__alignx
AM_CPPFLAGS += -Dinline=__inline
AM_CPPFLAGS += -DIMAGE_APPS_PROC
AM_CPPFLAGS += -DCDECL
AM_CPPFLAGS += -DT_ARM
AM_CPPFLAGS += -DNO_ARM_CLZ
AM_CPPFLAGS += -UENABLE_DEBUG_LOW
AM_CPPFLAGS += -DENABLE_DEBUG_HIGH
AM_CPPFLAGS += -DENABLE_DEBUG_ERROR
AM_CPPFLAGS += -UINPUT_BUFFER_LOG
AM_CPPFLAGS += -UOUTPUT_BUFFER_LOG
if TARGET_MSM8960
AM_CPPFLAGS += -DMAX_RES_1080P
AM_CPPFLAGS += -DMAX_RES_1080P_EBI
AM_CPPFLAGS += -UENABLE_GET_SYNTAX_HDR
AM_CPPFLAGS += -DUSE_ION
endif
AM_CPPFLAGS += -Iinc
AM_CPPFLAGS += -I../common/inc
AM_CPPFLAGS += -I../../../mm-core/inc
AM_CPPFLAGS += -I../../../mm-core/src/common

sources = src/omx_video_base.cpp
sources += src/omx_video_encoder.cpp
sources += src/video_encoder_device.cpp
sources += ../common/src/extra_data_handler.cpp

lib_LTLIBRARIES = libOmxVenc.la
libOmxVenc_la_SOURCES = $(sources)
libOmxVenc_la_CFLAGS = $(AM_CFLAGS) -fPIC
#libOmxVenc_la_LDLIBS = -lOmxCore -lstdc++ -lpthread
libOmxVenc_la_LDFLAGS = -shared -version-info $(OMXVIDEO_LIBRARY_VERSION)

bin_PROGRAMS = mm-venc-omx-test720p

mm_venc_omx_test720p_SOURCES := test/venc_test.cpp
mm_venc_omx_test720p_SOURCES += test/camera_test.cpp
mm_venc_omx_test720p_SOURCES += test/venc_util.c
mm_venc_omx_test720p_SOURCES += test/fb_test.c

mm_venc_omx_test720p_LDADD = -ldl -lpthread libOmxVenc.la
mm_venc_omx_test720p_LDADD += ../../../mm-core/libOmxCore.la