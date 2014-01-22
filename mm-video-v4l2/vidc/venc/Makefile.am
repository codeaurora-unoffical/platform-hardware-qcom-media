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
if TARGET_B_FAMILY
AM_CPPFLAGS += -DMAX_RES_1080P
AM_CPPFLAGS += -DMAX_RES_1080P_EBI
AM_CPPFLAGS += -DPROCESS_EXTRADATA_IN_OUTPUT_PORT
AM_CPPFLAGS += -DUSE_ION
AM_CPPFLAGS += -D_MSM8974_
AM_CPPFLAGS += -D_METAMODE_
AM_CPPFLAGS += -D_OPAQUE_
endif
if TARGET_MSM8226
AM_CPPFLAGS += -D_MSM8226_
endif

AM_CPPFLAGS += -I./inc
AM_CPPFLAGS += -I../common/inc
AM_CPPFLAGS += -I../../../libc2dcolorconvert
AM_CPPFLAGS += -I../../../mm-core/inc
AM_CPPFLAGS += -I../../../mm-core/src/common

sources = src/omx_video_base.cpp
sources += src/omx_video_encoder.cpp

if TARGET_MSM8960
sources += src/video_encoder_device.cpp
endif
if TARGET_B_FAMILY
sources += src/video_encoder_device_v4l2.cpp
endif
sources += ../common/src/extra_data_handler.cpp
sources += ../common/src/vidc_color_converter.cpp

lib_LTLIBRARIES = libOmxVenc.la
libOmxVenc_la_SOURCES = $(sources)
libOmxVenc_la_CFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) -fPIC
libOmxVenc_la_LDLIBS = -lOmxCore -lstdc++ -lpthread -lc2d2
libOmxVenc_la_LDLIBS += ../libc2d2colorconvert/libc2dcolorconvert.la
libOmxVenc_la_LDFLAGS = -shared -version-info $(OMXVIDEO_LIBRARY_VERSION)


bin_PROGRAMS = mm-venc-omx-test720p

mm_venc_omx_test720p_CPPFLAGS = -I./inc
mm_venc_omx_test720p_CPPFLAGS += -I../../../mm-core/inc
mm_venc_omx_test720p_CPPFLAGS += -I../common/inc

if TARGET_B_FAMILY
mm_venc_omx_test720p_CPPFLAGS += -DMAX_RES_1080P
mm_venc_omx_test720p_CPPFLAGS += -DMAX_RES_1080P_EBI
mm_venc_omx_test720p_CPPFLAGS += -DPROCESS_EXTRADATA_IN_OUTPUT_PORT
mm_venc_omx_test720p_CPPFLAGS += -D_MSM8974_
mm_venc_omx_test720p_CPPFLAGS += -DUSE_ION
mm_venc_omx_test720p_CPPFLAGS += -D_METAMODE_
endif

mm_venc_omx_test720p_SOURCES := test/venc_test.cpp
mm_venc_omx_test720p_SOURCES += test/camera_test.cpp
mm_venc_omx_test720p_SOURCES += test/venc_util.c
mm_venc_omx_test720p_SOURCES += test/fb_test.c
mm_venc_omx_test720p_LDADD = -ldl -lpthread -lOmxCore
mm_venc_omx_test720p_LDADD += ./libOmxVenc.la

