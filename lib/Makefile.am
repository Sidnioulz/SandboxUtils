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

AM_CPPFLAGS = -Wall $(DBUS_CFLAGS) $(GTK_CFLAGS) $(SYSTEMD_CFLAGS) $(GLIB_CFLAGS)
AM_LDFLAGS = $(DBUS_LIBS) $(GTK_LIBS) $(SYSTEMD_LIBS) $(GLIB_LIBS) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
lib_LTLIBRARIES = libsandboxutils.la

if DEBUG
		AM_CPPFLAGS += -ggdb3 -O0
else !DEBUG
		AM_CPPFLAGS += -O2 -DNDEBUG
endif !DEBUG

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
		$(SHELL) ./config.status libtool

## Building the library and include files
libsandboxutils_la_CPPFLAGS = -DG_LOG_DOMAIN=\"libsandboxutils\" $(AM_CPPFLAGS)

libsandboxutils_la_DEPENDENCIES = \
		$(GDBUS_GENERATED) \
		$(GLIB_MARSHAL_GENERATED)

libsandboxutils_la_SOURCES = \
		sandboxfilechooserdialog.c \
		localfilechooserdialog.c \
		remotefilechooserdialog.c \
		sandboxfilechooserdialogdbusobject.c \
		sandboxutilscommon.c\
		$(GLIB_MARSHAL_BODY)

libsandboxutils_la_HEADERS = \
		sandboxutils.h \
		sandboxutilscommon.h \
		localfilechooserdialog.h \
		remotefilechooserdialog.h \
		sandboxfilechooserdialog.h \
		sandboxfilechooserdialogdbusobject.h \
		$(GLIB_MARSHAL_HEADER)

libsandboxutils_ladir=$(includedir)/sandboxutils/

## GLib marshal generation
# glib-genmarshal sandboxutilsmarshals.list --body --prefix=sandboxutils_marshal > sandboxutilsmarshals.c
# glib-genmarshal sandboxutilsmarshals.list --header --prefix=sandboxutils_marshal > sandboxutilsmarshals.h
GLIB_MARSHAL_HEADER = sandboxutilsmarshals.h

$(GLIB_MARSHAL_HEADER): sandboxutilsmarshals.list Makefile
		$(GLIB_GENMARSHAL) --prefix=sandboxutils_marshal --header sandboxutilsmarshals.list > $(top_srcdir)/lib/sandboxutilsmarshals.h 2>&1

GLIB_MARSHAL_BODY = sandboxutilsmarshals.c

$(GLIB_MARSHAL_BODY): sandboxutilsmarshals.list Makefile
		$(GLIB_GENMARSHAL) --prefix=sandboxutils_marshal --body sandboxutilsmarshals.list > $(top_srcdir)/lib/sandboxutilsmarshals.c 2>&1

GLIB_MARSHAL_GENERATED= $(GLIB_MARSHAL_HEADER) $(GLIB_MARSHAL_BODY)

## GDBus interface generation
# gdbus-codegen sandboxfilechooserdialoginterface.xml --interface-prefix=org.mupuf.SandboxUtils. --generate-c-code sandboxfilechooserdialogdbusobject --c-generate-object-manager 
GDBUS_GENERATED = \
	sandboxfilechooserdialogdbusobject.h \
	sandboxfilechooserdialogdbusobject.c

$(GDBUS_GENERATED): sandboxfilechooserdialoginterface.xml Makefile
		$(PYTHON) $(GDBUS_CODEGEN) \
		--interface-prefix org.mupuf.SandboxUtils.SandboxFileChooserDialog \
		--c-generate-object-manager \
		--c-namespace SfcdDbusWrapper \
		--generate-c-code sandboxfilechooserdialogdbusobject \
		--generate-docbook sfcd-interface \
		$< \
		$(NULL)

dist_doc_DATA: $(GDBUS_GENERATED)
		$(CP) sfcd-interface-org.mupuf.SandboxUtils.SandboxFileChooserDialog.xml "$(realpath)/../docs/"
		
EXTRA_DIST += sandboxfilechooserdialoginterface.xml
BUILT_SOURCES += $(GDBUS_GENERATED) $(GLIB_MARSHAL_GENERATED) $(libsandboxutils_la_SOURCES)
CLEANFILES += $(GDBUS_GENERATED) $(GLIB_MARSHAL_GENERATED)
