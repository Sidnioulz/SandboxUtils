AM_CPPFLAGS = -Wall $(DBUS_CFLAGS) $(GTK_CFLAGS) $(SYSTEMD_CFLAGS)
AM_LDFLAGS = $(DBUS_LIBS) $(GTK_LIBS) $(SYSTEMD_LIBS)
bin_PROGRAMS = sud

sud_CPPFLAGS = -DG_LOG_DOMAIN=\"sud\" $(AM_CPPFLAGS)

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)

sud_SOURCES = server.c 

