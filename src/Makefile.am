AM_CPPFLAGS = -Wall $(DBUS_CFLAGS) $(GTK_CFLAGS) $(SYSTEMD_CFLAGS)
AM_LDFLAGS = $(DBUS_LIBS) $(GTK_LIBS) $(SYSTEMD_LIBS)
bin_PROGRAMS = sandboxutilsd

sandboxutilsd_CPPFLAGS = -DG_LOG_DOMAIN=\"sandboxutilsd\" $(AM_CPPFLAGS)

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)

sandboxutilsd_SOURCES = sandboxutilsd.c \
              sandboxutilsmarshals.c \
              sandboxutilsclientmanager.c \
              sandboxfilechooserdialog.c \
              sandboxfilechooserdialogdbuswrapper.c

