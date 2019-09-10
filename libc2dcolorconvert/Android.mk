LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

libc2d-def := -g -O3
libc2d-def += -Wno-parentheses
# Hypervisor
ifeq ($(ENABLE_HYP),true)
libc2d-def += -D_HYPERVISOR_
endif

LOCAL_SRC_FILES := \
        C2DColorConverter.cpp
LOCAL_CFLAGS := $(libc2d-def) -Werror
LOCAL_C_INCLUDES := \
    $(TARGET_OUT_HEADERS)/adreno
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qcom/display
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include

LOCAL_SHARED_LIBRARIES := liblog libdl

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libc2dcolorconvert

LOCAL_VENDOR_MODULE := true

LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

include $(BUILD_SHARED_LIBRARY)
