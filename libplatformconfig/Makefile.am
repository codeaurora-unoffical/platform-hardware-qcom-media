
AM_CPPFLAGS = -g0
AM_CPPFLAGS += -O3
AM_CPPFLAGS += -fpic
AM_CPPFLAGS += -Wno-deprecated-declarations
AM_CPPFLAGS += -Werror
AM_CPPFLAGS += -Wno-error=unused-variable
AM_CPPFLAGS += -w
AM_CPPFLAGS += -Wall
AM_CPPFLAGS += -Wextra
AM_CPPFLAGS += -fexceptions
AM_CPPFLAGS += -Wno-missing-field-initializers

AM_CPPFLAGS += -I$(top_srcdir)/mm-core/inc
AM_CPPFLAGS += -I$(top_srcdir)/mm-video-v4l2/vidc/common/inc/

sources = PlatformConfig.cpp
sources += ConfigParser.cpp

lib_LTLIBRARIES = libplatformconfig.la
libplatformconfig_la_SOURCES = $(sources)
libplatformconfig_la_CFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS) -fPIC
libplatformconfig_la_LDFLAGS = -llog -lutils -lcutils -lglib-2.0 -ldl -shared
libplatformconfig_la_LDFLAGS += -version-info 0

