AM_CPPFLAGS = -Wall $(DBUS_CFLAGS) $(GTK_CFLAGS)
AM_LDFLAGS = $(DBUS_LIBS) $(GTK_LIBS)
bin_PROGRAMS = sfad

sfad_CPPFLAGS = -DG_LOG_DOMAIN=\"sfad\" $(AM_CPPFLAGS)

BUILT_SOURCES = server-bindings.h 
# We don't want to install this header
noinst_HEADERS = $(BUILT_SOURCES)

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)

sfad_SOURCES = server.c server.h
