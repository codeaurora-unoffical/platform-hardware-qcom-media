# sources and intermediate files are separated

AM_CFLAGS = -Wall
AM_CFLAGS += -Wundef
AM_CFLAGS += -Wstrict-prototypes
AM_CFLAGS += -Wno-trigraphs
AM_CFLAGS += -g -O3
AM_CFLAGS += -O0 -fno-inline -fno-short-enums
AM_CFLAGS += -fPIC
AM_CFLAGS += -D_LINUX_


AM_CPPFLAGS = -U_ENABLE_QC_MSG_LOG_
AM_CPPFLAGS += -DVERBOSE
#AM_CPPFLAGS += -D_DSMODE_CMP_ -D_SECURE_CMP_
#AM_CPPFLAGS += -DVPP_PROPERTY
AM_CPPFLAGS += -I$(top_srcdir)/mm-core/inc
AM_CPPFLAGS += -I$(top_srcdir)/mm-core/src/common

h_sources =inc/OMX_Audio.h
h_sources +=inc/OMX_Component.h
h_sources +=inc/OMX_ContentPipe.h
h_sources +=inc/OMX_Core.h
h_sources +=inc/OMX_Image.h
h_sources +=inc/OMX_Index.h
h_sources +=inc/OMX_IVCommon.h
h_sources +=inc/OMX_Other.h
h_sources +=inc/OMX_QCOMExtns.h
h_sources +=inc/OMX_Types.h
h_sources +=inc/OMX_Video.h
h_sources +=inc/qc_omx_common.h
h_sources +=inc/qc_omx_component.h
h_sources +=inc/qc_omx_msg.h
h_sources +=inc/QOMX_AudioExtensions.h
h_sources +=inc/QOMX_AudioIndexExtensions.h
h_sources +=inc/OMX_CoreExt.h
h_sources +=inc/QOMX_CoreExtensions.h
h_sources +=inc/QOMX_FileFormatExtensions.h
h_sources +=inc/QOMX_IVCommonExtensions.h
h_sources +=inc/QOMX_SourceExtensions.h
h_sources +=inc/QOMX_VideoExtensions.h
h_sources +=inc/OMX_IndexExt.h
h_sources +=inc/QOMX_StreamingExtensions.h

c_sources =src/common/omx_core_cmp.cpp
c_sources +=src/common/qc_omx_core.c

#if TARGET_MSM8960
#c_sources +=src/8960/qc_registry_table.c
#endif

if TARGET_MSM8974
#c_sources +=src/msm8996/registry_table.c
c_sources +=src/msm8996/registry_table_android.c
endif

include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libOmxCore.la
libOmxCore_la_SOURCES = $(c_sources)
libOmxCore_la_LDFLAGS = -ldl -lrt -lpthread -shared
