plugin_LTLIBRARIES = libgsthist.la

libgsthist_la_SOURCES = gstvideohistogram.c gsthist.c gstdrawhist.c

libgsthist_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgsthist_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsthist_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(top_builddir)/gst-libs/gst/histogram/libgsthistmeta.la -lgsthistmeta

libgsthist_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstvideohistogram.h gstdrawhist.h

-include $(top_srcdir)/git.mk
