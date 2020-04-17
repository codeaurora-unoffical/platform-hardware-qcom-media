# Makefile.am - Automake script for mm-omxvideo
#
ACLOCAL_AMFLAGS = -I m4

BUILD_COMPONENTS := mm-core

if TARGET_HYPERVISOR
BUILD_COMPONENTS += hypv-intercept
endif

BUILD_COMPONENTS += libplatformconfig libc2dcolorconvert libarbitrarybytes

BUILD_COMPONENTS += mm-video-v4l2

SUBDIRS := $(BUILD_COMPONENTS)
