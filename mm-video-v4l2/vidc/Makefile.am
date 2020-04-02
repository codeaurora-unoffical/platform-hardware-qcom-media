# Makefile.am - Automake script for mm-omxvideo
#
ACLOCAL_AMFLAGS = -I m4

SUBDIRS :=

SUBDIRS += venc

# Enable video decoder compilation for other targets not for sdmsteppe concam
if WITH_VDEC
SUBDIRS += vdec
endif
