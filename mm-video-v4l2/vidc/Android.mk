LOCAL_PATH := $(call my-dir)
ifneq ($(TARGET_SUPPORTS_ANDROID_WEAR_KERNEL_4_14),true)
include $(call all-makefiles-under, $(LOCAL_PATH))
endif
