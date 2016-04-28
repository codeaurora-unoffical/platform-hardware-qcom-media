camerad_SOURCES =  src/camerad.cpp
camerad_SOURCES += src/qcamdaemon.cpp
camerad_SOURCES += src/camera_factory.cpp
camerad_SOURCES += src/virtual/camera_virtual.cpp
camerad_SOURCES += src/virtual/frame_scaler.cpp
camerad_SOURCES += src/omx/buffer_pool.cpp
camerad_SOURCES += src/omx/camera_component.cpp
camerad_SOURCES += src/omx/encoder_configure.cpp
camerad_SOURCES += src/omx/file_component.cpp
camerad_SOURCES += src/omx/preview_component.cpp
camerad_SOURCES += src/qcamvid_session.cpp
camerad_SOURCES += src/js_invoke.cpp
camerad_SOURCES += src/fpv_server.cpp
camerad_SOURCES += src/fpv_h264.cpp
camerad_SOURCES += src/pid_lock.cpp
camerad_SOURCES += src/json/js.c
camerad_SOURCES += src/json/jsgen.c
camerad_SOURCES += src/json/jstree.cpp

camerad_OBJS := $(camerad_SOURCES:%.c=%.o)
camerad_OBJS := $(camerad_OBJS:%.cpp=%.o)

camclient_SOURCES = src/qcamclient.cpp
camclient_OBJS = $(camclient_SOURCES:%.cpp=%.o)

CPPFLAGS += -std=c++11 -DHAVE_SYS_UIO_H
CPPFLAGS += -I $(SDKTARGETSYSROOT)/usr/include/live555
CPPFLAGS += -I $(SDKTARGETSYSROOT)/usr/include/omx
CPPFLAGS += -I $(SDKTARGETSYSROOT)/usr/include/linux-headers/usr/include
CPPFLAGS += -I src

LDFLAGS += -pthread -ldl
LDFLAGS += -lcamera
LDFLAGS += -lOmxVenc -lOmxCore
LDFLAGS += -lBasicUsageEnvironment -lgroupsock -lliveMedia -lUsageEnvironment

all: camerad camclient

camerad: $(camerad_OBJS)
	$(CXX) $(LDFLAGS) $(LOADLIBES) $(LDLIBS) -o $@ $^

camclient: $(camclient_SOURCES:%.cpp=%.o)
	$(CXX) $(LDFLAGS) $(LOADLIBES) $(LDLIBS) -o $@ $^

clean:
	rm -f camerad camclient $(camclient_OBJS) $(camerad_OBJS)
