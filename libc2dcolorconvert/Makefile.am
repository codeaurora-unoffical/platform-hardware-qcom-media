AM_CPPFLAGS = $(CPPFLAGS)
AM_CPPFLAGS += -Wall
AM_CPPFLAGS += -Wundef
AM_CPPFLAGS += -Wstrict-prototypes
AM_CPPFLAGS += -Wno-trigraphs
AM_CPPFLAGS += -g -O3
AM_CPPFLAGS += "-std=c++11"
AM_CPPFLAGS += -fPIC

c_sources = C2DColorConverter.cpp
h_sources = C2DColorConverter.h

lib_LTLIBRARIES = libc2dcolorconvert.la
include_HEADERS = $(h_sources)
libc2dcolorconvert_la_SOURCES = $(c_sources) $(h_sources)
libc2dcolorconvert_la_LDFLAGS = -ldl -llog -shared
