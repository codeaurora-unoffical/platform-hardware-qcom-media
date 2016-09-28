#                                               -*- Autoconf -*-

# configure.ac -- Autoconf script for MMCORE
#

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([MMCORE],
	1.0.0)
AM_INIT_AUTOMAKE([-Wall -Werror gnu foreign subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

#release versioning
MMCORE_MAJOR_VERSION=1
MMCORE_MINOR_VERSION=0
MMCORE_MICRO_VERSION=0

MMCORE_LIBRARY_VERSION=$MMCORE_MAJOR_VERSION:$MMCORE_MINOR_VERSION:$MMCORE_MICRO_VERSION
AC_SUBST(MMCORE_LIBRARY_VERSION)

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

AC_ARG_ENABLE([target-msm8610],
	AC_HELP_STRING([--enable-target-msm8610],
		[Enable conditional compile for target msm8610 [default=no]]),
	[target_msm8610="${enableval}"],
	target_msm8610=no)

AC_ARG_ENABLE([target-msm8226],
        AC_HELP_STRING([--enable-target-msm8226],
                [Enable conditional compile for target msm8226 [default=no]]),
        [target_msm8226="${enableval}"],
        target_msm8226=no)

AC_ARG_ENABLE([target-msm8916],
        AC_HELP_STRING([--enable-target-msm8916],
                [Enable conditional compile for target msm8916 [default=no]]),
        [target_msm8916="${enableval}"],
        target_msm8916=no)

AC_ARG_ENABLE([target-apq8016],
        AC_HELP_STRING([--enable-target-apq8016],
                [Enable conditional compile for target apq8016 [default=no]]),
        [target_apq8016="${enableval}"],
        target_apq8016=no)

AM_CONDITIONAL(TARGET_MSM8960, test "x$target_msm8960" = "xyes")
AM_CONDITIONAL(TARGET_MSM8974, test "x$target_msm8974" = "xyes")
AM_CONDITIONAL(TARGET_MSM8610, test "x$target_msm8610" = "xyes")
AM_CONDITIONAL(TARGET_MSM8226, test "x$target_msm8226" = "xyes")
AM_CONDITIONAL(TARGET_MSM8916, test "x$target_msm8916" = "xyes")
AM_CONDITIONAL(TARGET_APQ8016, test "x$target_apq8016" = "xyes")

AC_ARG_WITH([sanitized-headers],
	[AS_HELP_STRING([--with-sanitized-headers=DIR],[location of the sanitized Linux headers])],
	[CPPFLAGS="$CPPFLAGS -I$withval"])

AC_CONFIG_FILES([     \
	Makefile
	])
AC_OUTPUT
