AM_CPPFLAGS = -Wall $(DBUS_CFLAGS) $(GTK_CFLAGS) $(SYSTEMD_CFLAGS)
AM_LDFLAGS = $(DBUS_LIBS) $(GTK_LIBS) $(SYSTEMD_LIBS)
bin_PROGRAMS = sandboxutilsd

## SandboxUtilsD
sandboxutilsd_CPPFLAGS = -DG_LOG_DOMAIN=\"sandboxutilsd\" $(AM_CPPFLAGS) -ggdb

sandboxutilsd_SOURCES = sandboxutilsd.c \
		sandboxutilsmarshals.c \
		sandboxutilsclientmanager.c \
		sandboxfilechooserdialog.c \
		sandboxfilechooserdialogdbuswrapper.c \
		sandboxfilechooserdialogdbusobject.c \
		sandboxfilechooserdialogdbusobject.h

EXTRA_DIST=
BUILT_SOURCES = 
CLEANFILES =



## GDBus interface generation
# gdbus-codegen sandboxfilechooserdialoginterface.xml --interface-prefix=org.mupuf.SandboxUtils. --generate-c-code sandboxfilechooserdialogdbusobject --c-generate-object-manager 
GDBUS_GENERATED = \
	sandboxfilechooserdialogdbusobject.h \
	sandboxfilechooserdialogdbusobject.c

$(GDBUS_GENERATED) : sandboxfilechooserdialoginterface.xml Makefile
		$(PYTHON) gdbus-codegen \
		--interface-prefix org.mupuf.SandboxUtils.SandboxFileChooserDialog \
		--c-generate-object-manager \
		--c-namespace SfcdDbusWrapper \
		--generate-c-code sandboxfilechooserdialogdbusobject \
		--generate-docbook sfcd-interface \
		$< \
		$(NULL)

EXTRA_DIST += sandboxfilechooserdialoginterface.xml
BUILT_SOURCES += $(GDBUS_GENERATED)
CLEANFILES += $(GDBUS_GENERATED)



## Client library
#noinst_LTLIBRARIES = libsandboxutils.la
#libgdbus_example_objectmanager_la_SOURCES = sandboxfilechooserdialogdbusautogen.h sandboxfilechooserdialogdbusautogen.c
#libgdbus_example_objectmanager_la_LIBADD = 		\
#	$(top_builddir)/glib/libglib-2.0.la		\
#	$(top_builddir)/gobject/libgobject-2.0.la	\
#	$(top_builddir)/gmodule/libgmodule-2.0.la	\
#	$(top_builddir)/gio/libgio-2.0.la		\
#	$(NULL)

