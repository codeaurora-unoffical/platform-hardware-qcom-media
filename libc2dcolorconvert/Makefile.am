# sources and intermediate files are separated

AM_CPPFLAGS = -Wall
AM_CPPFLAGS += -Wundef
AM_CPPFLAGS += -Wstrict-prototypes
AM_CPPFLAGS += -Wno-trigraphs
AM_CPPFLAGS += -g -O3
AM_CPPFLAGS += -O0 -fno-inline -fno-short-enums
AM_CPPFLAGS += -fPIC
AM_CPPFLAGS += -D_LINUX_
AM_CPPFLAGS += -I$(top_srcdir)/mm-video-v4l2/vidc/common/inc

h_sources = C2DColorConverter.h
c_sources = C2DColorConverter.cpp

include_HEADERS = $(h_sources)
lib_LTLIBRARIES = libc2dcolorconvert.la
libc2dcolorconvert_la_SOURCES = $(c_sources)
libc2dcolorconvert_la_LDFLAGS = -ldl -llog -shared
