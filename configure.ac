
dnl required version of autoconf
AC_PREREQ([2.68])

AC_INIT([gst-plugins-qtec],[1.6.0])

dnl required versions of gstreamer and plugins-base
GST_REQUIRED=1.0
GSTPB_REQUIRED=1.0

dnl our libraries and install dirs use GST_API_VERSION in the filename
dnl to allow side-by-side installation of different API versions
GST_API_VERSION=1.0
AC_SUBST(GST_API_VERSION)
AC_DEFINE_UNQUOTED(GST_API_VERSION, "$GST_API_VERSION",
  [GStreamer API Version])


AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4 common/m4])

dnl required version of automake
AM_INIT_AUTOMAKE([1.10 foreign subdir-objects])

dnl enable mainainer mode by default
AM_MAINTAINER_MODE([enable])

dnl check for tools (compiler etc.)
AC_PROG_CC
AC_PROG_CXX
AC_PROG_OBJC
AM_PROG_AS

dnl required version of libtool
LT_PREREQ([2.2.6])
LT_INIT

dnl Check for the required version of GStreamer core (and gst-plugins-base)
dnl This will export GST_CFLAGS and GST_LIBS variables for use in Makefile.am
PKG_CHECK_MODULES(GST, [
  gstreamer-1.0 >= $GST_REQUIRED
  gstreamer-base-1.0 >= $GST_REQUIRED
  gstreamer-controller-1.0 >= $GST_REQUIRED
  gstreamer-video-1.0 >= $GST_REQUIRED
], [
  AC_SUBST(GST_CFLAGS)
  AC_SUBST(GST_OBJCFLAGS)
  AC_SUBST(GST_LIBS)
], [
  AC_MSG_ERROR([
      You need to install or upgrade the GStreamer development
      packages on your system. On debian-based systems these are
      libgstreamer1.0-dev and libgstreamer-plugins-base1.0-dev.
      on RPM-based systems gstreamer1-devel, libgstreamer1-devel
      or similar. The minimum version required is $GST_REQUIRED.
  ])
])

dnl GST_PLUGINS_BASE_CFLAGS
dnl prefer internal headers to already installed ones
dnl also add builddir include for enumtypes and marshal
GST_PLUGINS_BASE_CFLAGS="-I\$(top_srcdir)/gst-libs -I\$(top_builddir)/gst-libs"
AC_SUBST(GST_PLUGINS_BASE_CFLAGS)

dnl check if compiler understands -Wall (if yes, add -Wall to GST_CFLAGS)
AC_MSG_CHECKING([to see if compiler understands -Wall])
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([ ], [ ])], [
  GST_CFLAGS="$GST_CFLAGS -Wall"
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])

dnl warnings cause errors
GST_CFLAGS="$GST_CFLAGS -Werror-implicit-function-declaration -Werror"
GST_CXXFLAGS="$GST_CXXFLAGS -Werror-implicit-function-declaration -Werror"

dnl build static plugins or not
AC_MSG_CHECKING([whether to build static plugins or not])
AC_ARG_ENABLE(
  static-plugins,
  AC_HELP_STRING(
    [--enable-static-plugins],
    [build static plugins @<:@default=no@:>@]),
  [AS_CASE(
    [$enableval], [no], [], [yes], [],
    [AC_MSG_ERROR([bad value "$enableval" for --enable-static-plugins])])],
  [enable_static_plugins=no])
AC_MSG_RESULT([$enable_static_plugins])
if test "x$enable_static_plugins" = xyes; then
  AC_DEFINE(GST_PLUGIN_BUILD_STATIC, 1,
    [Define if static plugins should be built])
fi
AM_CONDITIONAL(GST_PLUGIN_BUILD_STATIC, test "x$enable_static_plugins" = "xyes")

dnl Release build
AC_MSG_CHECKING([whether to build debug or release])
AC_ARG_ENABLE(
  debug,
  AC_HELP_STRING(
    [--enable-debug],
    [build debug @<:@default=no@:>@]),
  [AS_CASE(
    [$enableval], [no], [], [yes], [],
    [AC_MSG_ERROR([bad value "$enableval" for --enable-debug])])],
  [enable_debug=no])
AC_MSG_RESULT([$enable_debug])
if test "x$enable_debug" = xyes; then
   AC_DEFINE(ENABLE_DEBUG, 1, [Define if debug should be built])
fi
AM_CONDITIONAL(ENABLE_DEBUG, test "x$enable_debug" = "xyes")

dnl set the plugindir where plugins should be installed (for src/Makefile.am)
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-1.0/plugins"
else
  plugindir="\$(libdir)/gstreamer-1.0"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

dnl conditionals for setting up variables

AC_PATH_PROG(VALGRIND_PATH, valgrind, no)
AM_CONDITIONAL(HAVE_VALGRIND, test ! "x$VALGRIND_PATH" = "xno")

AG_GST_CHECK_GST_CHECK($GST_API_VERSION, [$GST_REQUIRED], no)
AM_CONDITIONAL(HAVE_GST_CHECK, test "x$HAVE_GST_CHECK" = "xyes")

dnl adding this so common/check.mak remains unchanged
AM_CONDITIONAL(GST_GCOV_ENABLED, test ! "xno" = "xyes")

AC_CONFIG_FILES([
Makefile
gst/Makefile
gst/histogram/Makefile
gst/avgrow/Makefile
gst/avgframes/Makefile
gst/v4l2control/Makefile
gst/v4l2-pid/Makefile
gst/v4l2-sweep/Makefile
gst/fpncmagic/Makefile
gst/fpncsink/Makefile
gst-libs/Makefile
gst-libs/gst/Makefile
gst-libs/gst/v4l2/Makefile
gst-libs/gst/histogram/Makefile
gst-libs/gst/fpncmagic/Makefile
tests/Makefile
tests/files/Makefile
tests/check/Makefile
tests/examples/Makefile
])

AC_OUTPUT
