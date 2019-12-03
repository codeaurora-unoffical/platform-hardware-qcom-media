# Makefile.am - Automake script for mm-omxvideo
#
ACLOCAL_AMFLAGS = -I m4

SUBDIRS :=

if WITH_VENC
SUBDIRS += venc
endif

# Enable video decoder compilation for other targets not for APQ8009
if WITH_VDEC
SUBDIRS += vdec
endif
