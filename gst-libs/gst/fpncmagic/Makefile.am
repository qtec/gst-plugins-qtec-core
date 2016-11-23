lib_LTLIBRARIES = libgstfpncmagicmeta.la

CLEANFILES = $(BUILT_SOURCES)

libgstfpncmagicmeta_la_SOURCES = \
    gstfpncmagicmeta.c 

libgstfpncmagicmetaincludedir = $(includedir)/gstreamer/gst/fpncmagic

libgstfpncmagicmetainclude_HEADERS = \
    gstfpncmagicmeta.h 


libgstfpncmagicmeta_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)

libgstfpncmagicmeta_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(LIBM)
libgstfpncmagicmeta_la_LDFLAGS = $(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS) 



-include $(top_srcdir)/git.mk
