AM_CPPFLAGS = -Wall $(DBUS_CFLAGS) $(GTK_CFLAGS) $(SYSTEMD_CFLAGS)
AM_LDFLAGS = $(DBUS_LIBS) $(GTK_LIBS) $(SYSTEMD_LIBS)
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

libsandboxutils_la_SOURCES = \
		sandboxutilsmarshals.c \
		sandboxfilechooserdialog.c \
		localfilechooserdialog.c \
		sandboxfilechooserdialogdbusobject.c \
		sandboxutilscommon.c

libsandboxutils_la_HEADERS = \
		sandboxutilscommon.h \
		sandboxfilechooserdialog.h \
		sandboxfilechooserdialogdbusobject.h \
		sandboxutilsmarshals.h

libsandboxutils_ladir=$(includedir)/sandboxutils/

		
## GDBus interface generation
# gdbus-codegen sandboxfilechooserdialoginterface.xml --interface-prefix=org.mupuf.SandboxUtils. --generate-c-code sandboxfilechooserdialogdbusobject --c-generate-object-manager 
GDBUS_GENERATED = \
	sandboxfilechooserdialogdbusobject.h \
	sandboxfilechooserdialogdbusobject.c

$(GDBUS_GENERATED): sandboxfilechooserdialoginterface.xml Makefile
		$(PYTHON) gdbus-codegen \
		--interface-prefix org.mupuf.SandboxUtils.SandboxFileChooserDialog \
		--c-generate-object-manager \
		--c-namespace SfcdDbusWrapper \
		--generate-c-code sandboxfilechooserdialogdbusobject \
		--generate-docbook sfcd-interface \
		$< \
		$(NULL)

dist_doc_DATA: $(GDBUS_GENERATED)
		$(CP) sfcd-interface-org.mupuf.SandboxUtils.SandboxFileChooserDialog.xml "$(realpath)/../docs/"
		
EXTRA_DIST = sandboxfilechooserdialoginterface.xml
BUILT_SOURCES = $(GDBUS_GENERATED) $(libsandboxutils_la_SOURCES)
CLEANFILES = $(GDBUS_GENERATED)
