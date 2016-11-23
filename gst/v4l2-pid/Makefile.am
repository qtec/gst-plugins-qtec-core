plugin_LTLIBRARIES = libgstv4l2pid.la

libgstv4l2pid_la_SOURCES = gstv4l2pid.c

libgstv4l2pid_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstv4l2pid_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstv4l2pid_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(top_builddir)/gst-libs/gst/v4l2/libgstv4l2.la $(top_builddir)/gst-libs/gst/histogram/libgsthistmeta.la -lgsthistmeta

if !GST_PLUGIN_BUILD_STATIC
libgstv4l2pid_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstv4l2pid.h

-include $(top_srcdir)/git.mk
