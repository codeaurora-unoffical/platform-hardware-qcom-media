#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Requires autoconf tool later than 2.61
AC_PREREQ([2.61])
# Initialize the omxvideo package version 1.0.0
AC_INIT([omxvideo], 1.0.0)
# Does not strictly follow GNU Coding standards
AM_INIT_AUTOMAKE([gnu foreign subdir-objects])
AM_MAINTAINER_MODE
# defines some macros variable to be included by source
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

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

PKG_PROG_PKG_CONFIG

AC_ARG_ENABLE([target-msm8953],
        AC_HELP_STRING([--enable-target-msm8953],
                [Enable conditional compile for target msm8953 [default=no]]),
        [target_msm8953="${enableval}"])

AC_ARG_ENABLE([target-msm8909],
        AC_HELP_STRING([--enable-target-msm8909],
                [Enable conditional compile for target msm8909 [default=no]]),
        [target_msm8909="${enableval}"])

AC_ARG_ENABLE([target-msm8996],
        AC_HELP_STRING([--enable-target-msm8996],
                [Enable conditional compile for target msm8996 [default=no]]),
        [target_msm8996="${enableval}"])

AC_ARG_ENABLE([target-msm8610],
        AC_HELP_STRING([--enable-target-msm8610],
                [Enable conditional compile for target msm8610 [default=no]]),
        [target_msm8610="${enableval}"])

AC_ARG_ENABLE([target-msm8226],
        AC_HELP_STRING([--enable-target-msm8226],
                [Enable conditional compile for target msm8610 [default=no]]),
        [target_msm8226="${enableval}"])

AC_ARG_ENABLE([target-msm8937],
        AC_HELP_STRING([--enable-target-msm8937],
                [Enable conditional compile for target msm8937 [default=no]]),
        [target_msm8937="${enableval}"])

AC_ARG_ENABLE([is-ubwc-supported],
        AC_HELP_STRING([--enable-is-ubwc-supported],
                [Enable conditional compile for target msm8953 [default=no]]),
        [targets_that_support_ubwc="${enableval}"])

AC_ARG_ENABLE([targets-that-support-pq],
        AC_HELP_STRING([--enable-targets-that-support-pq],
                [Enable conditional compile for target msm8953 [default=no]]),
        [targets_that_support_pq="${enableval}"])

AC_ARG_ENABLE([targets-that-support-vqzip],
        AC_HELP_STRING([--enable-targets-that-support-vqzip],
                [Enable conditional compile for target msm8953 [default=no]]),
        [targets_that_support_vqzip="${enableval}"])

AC_ARG_ENABLE([target-uses-ion],
        AC_HELP_STRING([--enable-target-uses-ion],
                [Enable conditional compile for target target-uses-ion [default=no]]),
        [target_uses_ion="${enableval}"])

AC_ARG_ENABLE([targets-that-use-flag-msm8226],
        AC_HELP_STRING([--enable-targets-that-use-flag-msm8226],
                [Enable conditional compile for target targets_that_use_flag_msm8226 [default=no]]),
        [targets_that_use_flag_msm8226="${enableval}"])

AC_ARG_ENABLE([target-uses-media-extensions],
        AC_HELP_STRING([--enable-target-uses-media-extensions],
                [Enable conditional compile for target target_uses_media_extensions [default=no]]),
        [target_uses_media_extensions="${enableval}"])

AC_ARG_ENABLE([master-side-cp-target-list],
        AC_HELP_STRING([--enable-master-side-cp-target-list],
                [Enable conditional compile for target master_side_cp_target_list [default=no]]),
        [master_side_cp_target_list="${enableval}"])

AC_ARG_ENABLE([use-glib],
        AC_HELP_STRING([--enable-use-glib],
                [Enable conditional compile for use glib [default=no]]),
        [use_glib="${enableval}"])

AC_ARG_ENABLE([venc],
        AC_HELP_STRING([--enable-venc], [Build video encoder modules]), [venc="${enableval}"], [venc="no"])

AM_CONDITIONAL([WITH_VENC], [ test "x$venc" = "xyes"])

AC_ARG_ENABLE([vdec],
        AC_HELP_STRING([--enable-vdec], [Build video decoder modules]), [vdec="${enableval}"], [vdec="no"])

AM_CONDITIONAL([WITH_VDEC], [ test "x$vdec" = "xyes"])

AC_ARG_ENABLE([target-hypervisor],
	AC_HELP_STRING([--enable-target-hypervisor],
		[Enable conditional compile for target hypervisor [default=no]]),
	[target_hypervisor="${enableval}"],
	target_hypervisor=no)

AC_ARG_ENABLE([targets-use-libgbm],
        AC_HELP_STRING([--enable-targets-use-libgbm],
                [Enable conditional compile for target targets_use_libgbm [default=no]]),
        [targets_use_libgbm="${enableval}"])


AC_ARG_ENABLE([target-kernel-version],
        AC_HELP_STRING([--enable-target-kernel-version],
                [Enable conditional compile for target target_kernel_version [default=no]]),
        [target_kernel_version="${enableval}"])

AM_CONDITIONAL(TARGET_HYPERVISOR, test "x$target_hypervisor" = "xyes")

AC_ARG_WITH([sanitized-headers],
        [AS_HELP_STRING([--with-sanitized-headers=DIR],[location of the sanitized Linux kernel headers])],
        [CPPFLAGS="$CPPFLAGS -I $withval"])

AC_ARG_WITH([glib-headers],
        [AS_HELP_STRING([--with-glib-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([utils-headers],
        [AS_HELP_STRING([--with-utils-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([cutils-headers],
        [AS_HELP_STRING([--with-cutils-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([kernel-headers],
        [AS_HELP_STRING([--with-kernel-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([kernel-uapi-headers],
        [AS_HELP_STRING([--with-kernel-uapi-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([adreno-headers],
        [AS_HELP_STRING([--with-adreno-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([libgpustats-headers],
        [AS_HELP_STRING([--with-libgpustats-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([libpqstats-headers],
        [AS_HELP_STRING([--with-libpqstats-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([ui-headers],
        [AS_HELP_STRING([--with-ui-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([android-headers],
        [AS_HELP_STRING([--with-android-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([gralloc-headers],
        [AS_HELP_STRING([--with-gralloc-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([binder-headers],
        [AS_HELP_STRING([--with-binder-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([display-headers],
        [AS_HELP_STRING([--with-display-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([qdutils-headers],
        [AS_HELP_STRING([--with-qdutils-headers=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([glib-lib-dir],
        [AS_HELP_STRING([--with-glib-lib-dir=DIR],[location of common headers])],
        [CPPFLAGS="$CPPFLAGS -I$withval"])

AC_ARG_WITH([libvqzipstats-headers], [AC_HELP_STRING([--with-libvqzipstats-headers=DIR],
             [Location of vqzipstats headers])], [CPPFLAGS="${CPPFLAGS} -I${withval}"])

AM_CONDITIONAL(TARGET_MSM8953, [test "x$target_msm8953" = "xyes"])
AM_CONDITIONAL(TARGET_MSM8909, [test "x$target_msm8909" = "xyes"])
AM_CONDITIONAL(TARGET_MSM8996, [test "x$target_msm8996" = "xyes"])
AM_CONDITIONAL(TARGET_MSM8610, [test "x$target_msm8610" = "xyes"])
AM_CONDITIONAL(TARGET_MSM8226, [test "x$target_msm8226" = "xyes"])
AM_CONDITIONAL(TARGET_MSM8937, [test "x$target_msm8937" = "xyes"])
AM_CONDITIONAL(TARGETS_THAT_SUPPORT_UBWC, [test "x$targets_that_support_ubwc" = "xyes"])
AM_CONDITIONAL(TARGETS_THAT_SUPPORT_PQ, [test "x$targets_that_support_pq" = "xyes"])
AM_CONDITIONAL(TARGETS_THAT_SUPPORT_VQZIP, [test "x$targets_that_support_vqzip" = "xyes"])
AM_CONDITIONAL(TARGET_USES_ION, [test "x$target_uses_ion" = "xyes"])
AM_CONDITIONAL(TARGETS_THAT_USE_FLAG_MSM8226, [test "x$targets_that_use_flag_msm8226" = "xyes"])
AM_CONDITIONAL(TARGET_USES_MEDIA_EXTENSIONS, [test "x$target_uses_media_extensions" = "xyes"])
AM_CONDITIONAL(MASTER_SIDE_CP_TARGET_LIST, [test "x$master_side_cp_target_list" = "xyes"])
AM_CONDITIONAL(USE_GLIB, [test "x$use_glib" = "xyes"])
AM_CONDITIONAL(TARGET_USES_LIBGBM, [test "x$targets_use_libgbm" = "xyes"])
AM_CONDITIONAL(IS_KERNEL_VERSION_49, [test "x$target_kernel_version" = "x4.9"])

AC_ARG_WITH([glib],
      AC_HELP_STRING([--with-glib],
         [enable glib, building HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
        GLIB_CFLAGS="$GLIB_CFLAGS $GTHREAD_CFLAGS"
        GLIB_LIBS="$GLIB_LIBS $GTHREAD_LIBS"

        AC_SUBST(GLIB_CFLAGS)
        AC_SUBST(GLIB_LIBS)
fi

AC_SUBST([CPPFLAGS])
AC_SUBST([CFLAGS])

AC_CONFIG_FILES([ \
        Makefile \
	hypv-intercept/Makefile \
        mm-core/Makefile
        mm-video-v4l2/Makefile \
        mm-video-v4l2/vidc/Makefile \
        mm-video-v4l2/vidc/venc/Makefile \
        mm-video-v4l2/vidc/vdec/Makefile \
        ])
AC_OUTPUT
