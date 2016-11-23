lib_LTLIBRARIES = libgsthistmeta.la

CLEANFILES = $(BUILT_SOURCES)

libgsthistmeta_la_SOURCES = \
    gsthistmeta.c 

libgsthistmetaincludedir = $(includedir)/gstreamer/gst/histogram

libgsthistmetainclude_HEADERS = \
    gsthistmeta.h 


libgsthistmeta_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)

libgsthistmeta_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(LIBM)
libgsthistmeta_la_LDFLAGS = $(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS) 



-include $(top_srcdir)/git.mk
