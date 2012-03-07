LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	C2DColorConverterTest.cpp

LOCAL_C_INCLUDES:= $(TOP)/hardware/qcom/display/libgralloc \
		   $(TOP)/hardware/qcom/display/libqcomui \
		   $(TOP)/frameworks/base/include/media/stagefright \
		   $(TOP)/frameworks/base/include/media/stagefright/openmax \
		   $(TOP)/hardware/qcom/display/libcopybit \
		   $(TOP)/hardware/qcom/media/libc2dcolorconvert/

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder \
	libui \
	libgui \
	libdl \

LOCAL_MODULE:= test-C2D

LOCAL_MODULE_TAGS := tests

include $(BUILD_EXECUTABLE)
