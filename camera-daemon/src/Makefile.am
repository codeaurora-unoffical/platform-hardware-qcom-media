# g++ flags
AM_CXXFLAGS  = -std=c++11
AM_CXXFLAGS += -Wall -Werror
AM_CXXFLAGS += -Wno-format-extra-args
AM_CXXFLAGS += -Wno-format
AM_CXXFLAGS += -pthread
AM_CXXFLAGS += -DHAVE_SYS_UIO_H
AM_CXXFLAGS += -I../../mm-core/inc

camerad_SOURCES  = camerad.cpp
camerad_SOURCES += qcamdaemon.cpp
camerad_SOURCES += camera_factory.cpp
#camerad_SOURCES += camera_virtual.cpp
camerad_SOURCES += omx/buffer_pool.cpp
camerad_SOURCES += omx/camera_component.cpp
camerad_SOURCES += omx/encoder_configure.cpp
camerad_SOURCES += omx/file_component.cpp
camerad_SOURCES += omx/preview_component.cpp
camerad_SOURCES += qcamvid_session.cpp
camerad_SOURCES += fpv_server.cpp
camerad_SOURCES += fpv_h264.cpp
camerad_SOURCES += pid_lock.cpp
camerad_SOURCES += json/js.c
camerad_SOURCES += json/jsgen.c
camerad_SOURCES += json/jstree.cpp

camerad_LDADD   = -lcamera ../../mm-video-v4l2/vidc/venc/libOmxVenc.la \
                  ../../mm-core/libOmxCore.la $(live555LIBS)

# live555 libraries
live555LIBS = -lBasicUsageEnvironment -lgroupsock -lliveMedia -lUsageEnvironment

camerad_LDADD += $(live555LIBS)

camclient_SOURCES = qcamclient.cpp
camclient_LDFLAGS = -pthread

bin_PROGRAMS    = camerad camclient
