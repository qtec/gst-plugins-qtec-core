plugin_LTLIBRARIES = libgstavgframes.la

libgstavgframes_la_SOURCES = gstavgframes.c

libgstavgframes_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstavgframes_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstavgframes_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) 

libgstavgframes_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstavgframes.h

-include $(top_srcdir)/git.mk
