plugin_LTLIBRARIES = libgstv4l2sweep.la

libgstv4l2sweep_la_SOURCES = gstv4l2sweep.c

libgstv4l2sweep_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstv4l2sweep_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstv4l2sweep_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(top_builddir)/gst-libs/gst/v4l2/libgstv4l2.la $(top_builddir)/gst-libs/gst/fpncmagic/libgstfpncmagicmeta.la -lgstfpncmagicmeta

if !GST_PLUGIN_BUILD_STATIC
libgstv4l2sweep_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstv4l2sweep.h

-include $(top_srcdir)/git.mk
