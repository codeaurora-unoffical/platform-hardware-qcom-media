#                                               -*- Autoconf -*-

# configure.ac -- Autoconf script for mm-omxvideo
#

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([omxvideo],
	1.0.0)
AM_INIT_AUTOMAKE([-Wall -Werror gnu foreign subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

#release versioning
OMXVIDEO_MAJOR_VERSION=1
OMXVIDEO_MINOR_VERSION=0
OMXVIDEO_MICRO_VERSION=0

OMXVIDEO_LIBRARY_VERSION=$OMXVIDEO_MAJOR_VERSION:$OMXVIDEO_MINOR_VERSION:$OMXVIDEO_MICRO_VERSION
AC_SUBST(OMXVIDEO_LIBRARY_VERSION)

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AM_PROG_CC_C_O
AM_PROG_AR
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_ARG_WITH([sanitized-headers],
	[AS_HELP_STRING([--with-sanitized-headers=DIR],[location of the sanitized Linux headers])],
	[CPPFLAGS="$CPPFLAGS -I$withval"])

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, dummy=yes,
	AC_MSG_ERROR(GLib >= 2.16 is required))
GLIB_LIBS="$GLIB_LIBS $GTHREAD_LIBS"
AC_SUBST(GLIB_LIBS)

AC_CONFIG_FILES([
	Makefile \
	mm-core/Makefile \
	mm-video-v4l2/vidc/venc/Makefile \
	libc2dcolorconvert/Makefile \
	camera-daemon/Makefile \
	camera-daemon/src/Makefile
	])
AC_OUTPUT
