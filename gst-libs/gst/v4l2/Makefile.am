lib_LTLIBRARIES = libgstv4l2.la

CLEANFILES = $(BUILT_SOURCES)

libgstv4l2_la_SOURCES = \
    gstv4l2Events.c \
    gstv4l2Queries.c \
    gstv4l2commonutils.c \
    gstv4l2debug.c

libgstv4l2includedir = $(includedir)/gstreamer/gst/v4l2

libgstv4l2include_HEADERS = \
    gstv4l2Events.h \
    gstv4l2Queries.h \
    gstv4l2commonutils.h \
    gstv4l2debug.h \
    gstv4l2lib.h

libgstv4l2_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)

libgstv4l2_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(LIBM)
libgstv4l2_la_LDFLAGS = $(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS)

-include $(top_srcdir)/git.mk
