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


AM_CPPFLAGS = 					\
	@SU_DEBUG_FLAGS@ 			\
	-DG_DISABLE_DEPRECATED \
  $(DBUS_CFLAGS) \
  $(GTK_CFLAGS) \
  $(SYSTEMD_CFLAGS) \
  $(GLIB_CFLAGS) \
  -I$(top_srcdir)/lib

AM_LDFLAGS = $(DBUS_LIBS) $(GTK_LIBS) $(SYSTEMD_LIBS) $(GLIB_LIBS) $(libsandboxutils_LIBS)

bin_PROGRAMS = sandboxutilsd

## now managed in 
##if DEBUG
##  AM_CPPFLAGS += -ggdb3 -O0
##else !DEBUG
##  AM_CPPFLAGS += -O2 -DNDEBUG
##endif !DEBUG

## SandboxUtilsD
sandboxutilsd_CPPFLAGS = -DG_LOG_DOMAIN=\"sandboxutilsd\" $(AM_CPPFLAGS)

sandboxutilsd_SOURCES = sandboxutilsd.c \
		sandboxutilsclientmanager.c \
		sandboxfilechooserdialogdbuswrapper.c \
		$(top_srcdir)/lib/sandboxutilsmarshals.c \
		$(top_srcdir)/lib/sandboxfilechooserdialog.c \
		$(top_srcdir)/lib/sandboxfilechooserdialogdbusobject.c \
		$(top_srcdir)/lib/sandboxfilechooserdialogdbusobject.h

