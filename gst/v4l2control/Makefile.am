plugin_LTLIBRARIES = libgstv4l2control.la

libgstv4l2control_la_SOURCES = gstv4l2control.c gstv4l2.c gstv4l2utils.c

libgstv4l2control_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstv4l2control_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstv4l2control_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(top_builddir)/gst-libs/gst/v4l2/libgstv4l2.la -lv4l2

libgstv4l2control_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstv4l2control.h gstv4l2utils.h

-include $(top_srcdir)/git.mk
