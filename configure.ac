#                                               -*- Autoconf -*-

# configure.ac -- Autoconf script for mm-omxvideo
#

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([omxvideo],
	1.1.0)
AM_INIT_AUTOMAKE([-Wall -Werror gnu foreign subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

#release versioning
OMXVIDEO_MAJOR_VERSION=1
OMXVIDEO_MINOR_VERSION=1
OMXVIDEO_MICRO_VERSION=0

OMXVIDEO_LIBRARY_VERSION=$OMXVIDEO_MAJOR_VERSION:$OMXVIDEO_MINOR_VERSION:$OMXVIDEO_MICRO_VERSION
AC_SUBST(OMXVIDEO_LIBRARY_VERSION)

# Checks for programs.
AM_PROG_AR
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_ARG_ENABLE([target-msm8960],
	AC_HELP_STRING([--enable-target-msm8960],
		[Enable conditional compile for target msm8960 [default=no]]),
	[target_msm8960="${enableval}"],
	target_msm8960=no)

AC_ARG_ENABLE([target-msm8974],
	AC_HELP_STRING([--enable-target-msm8974],
		[Enable conditional compile for target msm8974 [default=no]]),
	[target_msm8974="${enableval}"],
	target_msm8974=no)

AM_CONDITIONAL(TARGET_MSM8960, test "x$target_msm8960" = "xyes")
AM_CONDITIONAL(TARGET_MSM8974, test "x$target_msm8974" = "xyes")

AC_ARG_WITH([sanitized-headers],
	[AS_HELP_STRING([--with-sanitized-headers=DIR],[location of the sanitized Linux headers])],
	[CPPFLAGS="$CPPFLAGS -I$withval -I${withval}/C2D"])

AC_ARG_WITH([common-includes],
	[AS_HELP_STRING([--with-common-include=DIR],[location of common headers])],
	[CPPFLAGS="$CPPFLAGS -I$withval -I${withval}/C2D"])

AC_ARG_WITH([libhardware-headers],
	[AS_HELP_STRING([--with-libhardware-headers=DIR],[location of the libhardware headers])],
	[CPPFLAGS="$CPPFLAGS -I${withval}/modules/gralloc"])

AC_SUBST([CFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([CC])

AC_CONFIG_FILES([     \
	Makefile \
	mm-core/Makefile \
	mm-video-v4l2/Makefile \
	mm-video-v4l2/vidc/Makefile \
	mm-video-v4l2/vidc/vdec/Makefile
	])
AC_OUTPUT
