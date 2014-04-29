#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(sfad-server, 0.2, sidnioulz@gmail.com)
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])


SYSTEMD_REQUIRED=212

# Checks for programs.
AC_PROG_CC
PKG_PROG_PKG_CONFIG

# Checks for libraries.
PKG_CHECK_MODULES([DBUS], [dbus-glib-1])
PKG_CHECK_MODULES([GTK], [gtk+-3.0])
PKG_CHECK_MODULES(SYSTEMD, [ libsystemd >= $SYSTEMD_REQUIRED ])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.


AS_AC_EXPAND(DATADIR, $datadir)
AS_AC_EXPAND(LIBDIR, $libdir)

DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])


# pkg-config systemd --variable=systemdsystemunitdir would tell me the correct
# path but then I'd need to truncate the prefix and replace it by LIBDIR
SYSTEMD_USER_DIR="$LIBDIR/systemd/user"
SYSTEMD_SYSTEM_DIR="$LIBDIR/systemd/system"
AC_SUBST(SYSTEMD_USER_DIR)
AC_SUBST(SYSTEMD_SYSTEM_DIR)
AC_DEFINE_UNQUOTED(SYSTEMD_USER_DIR, "$SYSTEMD_USER_DIR", [Where user services dir for Systemd is])
AC_DEFINE_UNQUOTED(SYSTEMD_SYSTEM_DIR, "$SYSTEMD_SYSTEM_DIR", [Where system services dir for Systemd is])


AM_INIT_AUTOMAKE([subdir-objects -Wall])

# LT_PREREQ([2.2])
# LT_INIT([dlopen])

AM_PROG_CC_C_O

AC_CONFIG_FILES([Makefile
                 data/Makefile
                 src/Makefile])
AC_OUTPUT
