AM_CFLAGS = -Wall
AM_CFLAGS += -Wundef
AM_CFLAGS += -Wstrict-prototypes
AM_CFLAGS += -Wno-trigraphs
AM_CFLAGS += -g -O3 -Dlrintf=_ffix_r

AM_CPPFLAGS = -D__align=__alignx
AM_CPPFLAGS += -D__alignx\(x\)=__attribute__\(\(__aligned__\(x\)\)\)
AM_CPPFLAGS += -Dinline=__inline
AM_CPPFLAGS += -DCDECL
AM_CPPFLAGS += -DT_ARM
AM_CPPFLAGS += -UENABLE_DEBUG_LOW
AM_CPPFLAGS += -DENABLE_DEBUG_HIGH
AM_CPPFLAGS += -DENABLE_DEBUG_ERROR
AM_CPPFLAGS += -UINPUT_BUFFER_LOG
AM_CPPFLAGS += -UOUTPUT_BUFFER_LOG
AM_CPPFLAGS += -D_ANDROID_ICS_

#AM_CPPFLAGS += -Iinc
#AM_CPPFLAGS += -I../common/inc
#AM_CPPFLAGS += -I../../../mm-core/inc
#AM_CPPFLAGS += -I../../../mm-core/src/common
#AM_CPPFLAGS += -I../../../mm-core/src/common
#AM_CPPFLAGS += -I../../../libstagefrighthw/

AM_CPPFLAGS += -I${top_srcdir}/mm-video-v4l2/vidc/venc/inc
AM_CPPFLAGS += -I${top_srcdir}/mm-video-v4l2/vidc/common/inc
AM_CPPFLAGS += -I${top_srcdir}/mm-core/inc
AM_CPPFLAGS += -I${top_srcdir}/mm-core/src/common
AM_CPPFLAGS += -I${top_srcdir}/libstagefrighthw
AM_CPPFLAGS += -I${top_srcdir}/libc2dcolorconvert
#AM_CPPFLAGS += -I${WORKSPACE}/vendor/qcom/proprietary/mm-video-noship/vidc/perceptual-qp/inc
AM_CPPFLAGS += `pkg-config --cflags glib-2.0`

AM_CPPFLAGS += -DMAX_RES_1080P
AM_CPPFLAGS += -DMAX_RES_1080P_EBI
AM_CPPFLAGS += -DPROCESS_EXTRADATA_IN_OUTPUT_PORT
AM_CPPFLAGS += -D_MSM8974_
AM_CPPFLAGS += -D_MSM8909_
AM_CPPFLAGS += -DUSE_ION

sources = src/omx_video_base.cpp
sources += src/omx_video_encoder.cpp
sources += src/video_encoder_device_v4l2.cpp
sources += ../common/src/extra_data_handler.cpp

lib_LTLIBRARIES = libOmxVenc.la
libOmxVenc_la_SOURCES = $(sources)
libOmxVenc_la_CFLAGS = $(AM_CFLAGS) -fPIC
libOmxVenc_la_LDFLAGS = -shared -version-info $(OMXVIDEO_LIBRARY_VERSION) @GLIB_LIBS@ -lcutils
