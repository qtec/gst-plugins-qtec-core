include $(top_srcdir)/common/check.mak

TEST_FILES_DIRECTORY = $(top_srcdir)/tests/files

REGISTRY_ENVIRONMENT = \
	GST_REGISTRY_1_0=$(CHECK_REGISTRY)

TESTS_ENVIRONMENT = \
	$(REGISTRY_ENVIRONMENT)					\
	GST_PLUGIN_SYSTEM_PATH_1_0=				\
	GST_PLUGIN_PATH_1_0=$(top_builddir)/gst:$(top_builddir)/ext:$(top_builddir)/sys:$(GSTPB_PLUGINS_DIR):$(GST_PLUGINS_DIR)			\
	GST_PLUGIN_LOADING_WHITELIST="gstreamer@$(GST_PLUGINS_DIR):gst-plugins-base@$(GSTPB_PLUGINS_DIR):gst-plugins-qtec@$(top_builddir)"	\
	GST_STATE_IGNORE_ELEMENTS=""

# fake device drivers: we could run hardware element tests against dummy drivers
# v4l2: vivo (part of normal kernel)
#   modprobe vivo;
#   gst-launch v4l2src device="/dev/video1" ! xvimagesink;
#   rmmod vivo
#
# alsa: snd-dummy (part of normal alsa, not removable)
#   modprobe snd-dummy;
#   gst-launch alsasrc device="hw:2" ! fakesink
#   gst-launch fakesrc ! alsasink device="hw:2"
#
# need a way to figure out value for the device property

# the core dumps of some machines have PIDs appended
CLEANFILES = core.* test-registry.*

clean-local: clean-local-check
distclean-local: distclean-local-orc

# Keeping one case as an example
# if USE_PLUGIN_WAVENC
# check_wavenc = pipelines/wavenc
# else
# check_wavenc =
# endif

# All tests should go under here
check_PROGRAMS = \
	elements/v4l2control \
	elements/avgframes \
	elements/avgrow \
	elements/fpncmagic \
	elements/fpncsink \
	elements/histogram

testbenchdir = $(datadir)/gstreamer1.0-plugins-qtec
testbench_PROGRAMS = $(check_PROGRAMS)

VALGRIND_TO_FIX =

TESTS = $(check_PROGRAMS)

AM_CFLAGS = $(GST_OBJ_CFLAGS) $(GST_CHECK_CFLAGS) $(CHECK_CFLAGS) \
	$(GST_OPTION_CFLAGS) $(GST_CFLAGS) -DGST_TEST_FILES_PATH="\"$(TEST_FILES_DIRECTORY)\"" \
	-UG_DISABLE_ASSERT -UG_DISABLE_CAST_CHECKS
LDADD = $(GST_OBJ_LIBS) $(GST_CHECK_LIBS) $(CHECK_LIBS)

# valgrind testing
VALGRIND_TESTS_DISABLE = \
	$(VALGRIND_TO_FIX)

SUPPRESSIONS = $(top_srcdir)/common/gst.supp


# v4l2utils utility lib
noinst_LTLIBRARIES = libv4l2utils.la
libv4l2utils_la_SOURCES = elements/v4l2utils/v4l2utils.c \
	elements/v4l2utils/v4l2utils.h
libv4l2utils_la_CFLAGS = \
	-I$(top_srcdir)/tests/check \
	$(GST_CHECK_CFLAGS) $(GST_OPTION_CFLAGS) -DGST_USE_UNSTABLE_API

elements_v4l2control_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) $(AM_CFLAGS)
elements_v4l2control_LDADD = $(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) $(GST_LIBS) \
	libv4l2utils.la \
	$(LDADD) \
	$(top_builddir)/gst-libs/gst/v4l2/.libs/libgstv4l2.so


elements_avgframes_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) $(AM_CFLAGS)
elements_avgframes_LDADD = $(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) $(GST_LIBS) $(LDADD)

elements_avgrow_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) $(AM_CFLAGS)
elements_avgrow_LDADD = $(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) $(GST_LIBS) $(LDADD)

elements_fpncmagic_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) $(AM_CFLAGS)
elements_fpncmagic_LDADD = $(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) $(GST_LIBS) $(LDADD) $(top_builddir)/gst-libs/gst/fpncmagic/.libs/libgstfpncmagicmeta.so

elements_fpncsink_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) $(AM_CFLAGS)
elements_fpncsink_LDADD = $(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) $(GST_LIBS) \
	libv4l2utils.la \
	 $(LDADD) \
	$(top_builddir)/gst-libs/gst/v4l2/.libs/libgstv4l2.so

elements_histogram_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) $(AM_CFLAGS)
elements_histogram_LDADD = $(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) $(GST_LIBS) $(LDADD) \
	$(top_builddir)/gst-libs/gst/histogram/.libs/libgsthistmeta.so

EXTRA_DIST =

-include $(top_srcdir)/git.mk
