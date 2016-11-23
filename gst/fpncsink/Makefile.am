plugin_LTLIBRARIES = libgstfpncsink.la

libgstfpncsink_la_SOURCES = gstfpncsink.c

libgstfpncsink_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstfpncsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstfpncsink_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(top_builddir)/gst-libs/gst/histogram/libgsthistmeta.la $(top_builddir)/gst-libs/gst/v4l2/libgstv4l2.la -lgsthistmeta $(top_builddir)/gst-libs/gst/fpncmagic/libgstfpncmagicmeta.la -lgstfpncmagicmeta

libgstfpncsink_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS) 

noinst_HEADERS = gstfpncsink.h

-include $(top_srcdir)/git.mk
