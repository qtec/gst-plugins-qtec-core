plugin_LTLIBRARIES = libgstfpncmagic.la

libgstfpncmagic_la_SOURCES = gstfpncmagic.c quickselect.c

libgstfpncmagic_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstfpncmagic_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstfpncmagic_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(top_builddir)/gst-libs/gst/fpncmagic/libgstfpncmagicmeta.la -lgstfpncmagicmeta

libgstfpncmagic_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstfpncmagic.h

-include $(top_srcdir)/git.mk
