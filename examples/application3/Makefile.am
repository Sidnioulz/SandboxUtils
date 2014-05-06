# initialize variables for unconditional += appending
BUILT_SOURCES =
BUILT_EXTRA_DIST =
CLEANFILES = *.log *.trs
DISTCLEANFILES =
MAINTAINERCLEANFILES =
EXTRA_DIST =
TEST_PROGS =

noinst_LTLIBRARIES =
noinst_PROGRAMS = exampleapp
noinst_SCRIPTS =
noinst_DATA =

check_LTLIBRARIES =
check_PROGRAMS =
check_SCRIPTS =
check_DATA =

AM_CPPFLAGS = 					\
	@SU_DEBUG_FLAGS@ 			\
	-DG_DISABLE_DEPRECATED \
  $(DBUS_CFLAGS) \
  $(GTK_CFLAGS) \
  $(GLIB_CFLAGS) \
  -I$(top_srcdir)/lib

AM_LDFLAGS = $(DBUS_LIBS) $(GTK_LIBS) $(GLIB_LIBS)

exampleapp_CPPFLAGS = -DG_LOG_DOMAIN=\"sandboxutilsexampleapp\" $(AM_CPPFLAGS)

exampleapp_LDADD = $(top_srcdir)/lib/libsandboxutils.la
exampleapp_DEPENDENCIES = $(top_srcdir)/lib/libsandboxutils.la

exampleapp_SOURCES = \
	main.c \
	exampleapp.c exampleapp.h \
	exampleappwin.c exampleappwin.h \
	resources.c

BUILT_SOURCES += resources.c

resources.c: exampleapp.gresource.xml window.ui
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/exampleapp.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --generate-source

EXTRA_DIST += \
	window.ui \
	exampleapp.gresource.xml
