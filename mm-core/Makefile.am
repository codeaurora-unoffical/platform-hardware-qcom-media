# sources and intermediate files are separated

AM_CFLAGS = -Wall
AM_CFLAGS += -Wundef
AM_CFLAGS += -Wstrict-prototypes
AM_CFLAGS += -Wno-trigraphs
AM_CFLAGS += -g -O3
AM_CFLAGS += -O0 -fno-inline -fno-short-enums
AM_CFLAGS += -fPIC

AM_CPPFLAGS = -D_ENABLE_QC_MSG_LOG_
AM_CPPFLAGS += -I${top_srcdir}/mm-core/inc
AM_CPPFLAGS += -I${top_srcdir}/mm-core/src/common

#AM_CPPFLAGS += -I/inc
#AM_CPPFLAGS += -I/src/common

MM_CORE_TARGET = 8909

h_sources = inc/OMX_Audio.h
h_sources += inc/OMX_Component.h
h_sources += inc/OMX_ContentPipe.h
h_sources += inc/OMX_Core.h
h_sources += inc/OMX_Image.h
h_sources += inc/OMX_Index.h
h_sources += inc/OMX_IVCommon.h
h_sources += inc/OMX_Other.h
h_sources += inc/OMX_QCOMExtns.h
h_sources += inc/OMX_Types.h
h_sources += inc/OMX_Video.h
h_sources += inc/qc_omx_common.h
h_sources += inc/qc_omx_component.h
h_sources += inc/qc_omx_msg.h
h_sources += inc/QOMX_AudioExtensions.h
h_sources += inc/QOMX_AudioIndexExtensions.h
h_sources += inc/OMX_CoreExt.h
h_sources += inc/QOMX_CoreExtensions.h
h_sources += inc/QOMX_FileFormatExtensions.h
h_sources += inc/QOMX_IVCommonExtensions.h
h_sources += inc/QOMX_SourceExtensions.h
h_sources += inc/QOMX_VideoExtensions.h
h_sources += inc/OMX_IndexExt.h
h_sources += inc/OMX_VideoExt.h
h_sources += inc/QOMX_StreamingExtensions.h
h_sources += inc/QCMediaDefs.h
h_sources += inc/QCMetaData.h
h_sources += src/common/omx_core_cmp.h
h_sources += src/common/qc_omx_core.h

c_sources = src/common/omx_core_cmp.cpp
c_sources += src/common/qc_omx_core.c
c_sources += src/$(MM_CORE_TARGET)/registry_table.c

# It's being copied by recipe now
#include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libOmxCore.la
libOmxCore_la_SOURCES = $(c_sources)
#libOmxCore_la_CPPFLAGS = $(AM_CPPFLAGS)
libOmxCore_la_LDFLAGS = -ldl -lrt -lpthread -shared -version-info $(OMXVIDEO_LIBRARY_VERSION)
