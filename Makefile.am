SUBDIRS = gst-libs gst tests

ACLOCAL_AMFLAGS = -I m4

DISTCLEANFILES = aclocal.m4 compile config.guess config.h.in config.sub \
	depcomp gtk-doc.make m4 install-sh ltmain.sh missing

CLEANFILES = build out

if HAVE_GST_CHECK
check-torture:
	$(MAKE) -C tests/check torture
build-checks:
	$(MAKE) -C tests/check build-checks
else
check-torture:
	true
build-checks:
	true
endif

GITIGNOREFILES = gst-build-* .build_successful_tag_file.txt

-include $(top_srcdir)/git.mk
