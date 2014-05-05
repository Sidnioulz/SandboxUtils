## Makefile.am stolen and adapted from GLib
# initialize variables for unconditional += appending
BUILT_SOURCES =
BUILT_EXTRA_DIST =
CLEANFILES = *.log *.trs
DISTCLEANFILES =
MAINTAINERCLEANFILES =
EXTRA_DIST =
TEST_PROGS =

noinst_LTLIBRARIES =
noinst_PROGRAMS =
noinst_SCRIPTS =
noinst_DATA =

check_LTLIBRARIES =
check_PROGRAMS =
check_SCRIPTS =
check_DATA =

ACLOCAL_AMFLAGS = -I m4macros ${ACLOCAL_FLAGS}

SUBDIRS = . lib server # TODO docs tests
DIST_SUBDIRS = $(SUBDIRS) # TODO? build

AM_CPPFLAGS = 					\
	@SU_DEBUG_FLAGS@ 			\
	-DG_DISABLE_DEPRECATED \
  $(DBUS_CFLAGS) \
  $(GTK_CFLAGS) \
  $(SYSTEMD_CFLAGS) \
  $(GLIB_CFLAGS)

EXTRA_DIST += 			\
	autogen.sh		\
	sandboxutils.pc.in 		\
	gtk-doc.make \
	ChangeLog

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sandboxutils.pc

$(pkgconfig_DATA): config.status

# build documentation when doing distcheck
DISTCHECK_CONFIGURE_FLAGS = --enable-debug --enable-gtk-doc --enable-man --disable-maintainer-mode

DISTCLEANFILES += config.lt aclocal.m4 autom4te.cache compile \
		config.guess config.h.in config.sub \
		depcomp install-sh ltmain.sh missing

distclean-local: 
	if test $(srcdir) = .; then :; else \
	    rm -f $(BUILT_EXTRA_DIST); \
	fi

.PHONY: files release snapshot ChangeLog

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	rm -rf .deps */.deps
	cd docs && make sandboxutils.html
	$(MAKE) distcheck

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

snapcheck:
	$(MAKE) distcheck distdir=$(PACKAGE)-snap`date +"%Y%m%d"`

dist-hook: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)'; \
	for f in $$files; do \
	  if test -f $$f; then d=.; else d=$(srcdir); fi; \
	  rm -f $(distdir)/$$f && cp $$d/$$f $(distdir) || exit 1; done
