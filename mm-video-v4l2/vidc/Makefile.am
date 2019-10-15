# Makefile.am - Automake script for mm-omxvideo
#
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = venc

# Enable video decoder compilation for other targets not for APQ8009
if !SKIP_VDEC_BUILD
SUBDIRS += vdec
endif
