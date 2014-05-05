# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.62])

dnl ***********************************
dnl *** include special GLib macros ***
dnl ***********************************

m4_define(glib_configure_ac)

# Sandbox Utils configure.ac (stolen and adapted from the GLib)
#
# if any functions have been added, set su_interface_age to 0.
# if backwards compatibility has been broken,
# set su_binary_age _and_ su_interface_age to 0.
#
# in easier to understand terms:
#

m4_define([su_major_version], [0])
m4_define([su_minor_version], [4])
m4_define([su_micro_version], [9])
m4_define([su_interface_age], [0])
m4_define([su_binary_age], [0])

m4_define([su_version],
          [su_major_version.su_minor_version.su_micro_version])
# libtool version related macros
m4_define([su_lt_release], [su_major_version])
m4_define([su_lt_current], [su_minor_version])
m4_define([su_lt_revision], [su_micro_version])
m4_define([su_lt_age], [m4_eval(su_binary_age - su_interface_age)])

# debug is on by default - no odd/even numbers here
m4_define([su_debug_default], [yes])


AC_INIT(sandboxutils, [su_version],
        [http://mupuf.org/project/sandboxutils.html])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([lib/sandboxutils.h])
AC_CONFIG_MACRO_DIR([m4macros])

# Save this value here, since automake will set cflags later
cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([-Wall -Wno-extra-portability])
AM_MAINTAINER_MODE([disable])

SU_MAJOR_VERSION=su_major_version
SU_MINOR_VERSION=su_minor_version
SU_MICRO_VERSION=su_micro_version
SU_INTERFACE_AGE=su_interface_age
SU_BINARY_AGE=su_binary_age
SU_VERSION=su_version

AC_SUBST(GU_MAJOR_VERSION)
AC_SUBST(GU_MINOR_VERSION)
AC_SUBST(GU_MICRO_VERSION)
AC_SUBST(GU_VERSION)
AC_SUBST(GU_INTERFACE_AGE)
AC_SUBST(GU_BINARY_AGE)

AC_DEFINE(GU_MAJOR_VERSION, [su_major_version],
	  [Define to the SandboxUtils major version])
AC_DEFINE(SU_MINOR_VERSION, [su_minor_version],
	  [Define to the SandboxUtils minor version])
AC_DEFINE(SU_MICRO_VERSION, [su_micro_version],
	  [Define to the SandboxUtils micro version])
AC_DEFINE(SU_INTERFACE_AGE, [su_interface_age],
	  [Define to the SandboxUtils interface age])
AC_DEFINE(SU_BINARY_AGE, [su_binary_age],
	  [Define to the SandboxUtils binary age])

# libtool versioning
LT_RELEASE=su_lt_release
LT_CURRENT=su_lt_current
LT_REVISION=su_lt_revision
LT_AGE=su_lt_age
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl Checks for programs.
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
PKG_PROG_PKG_CONFIG

AM_CONDITIONAL(HAVE_GCC, [test "$GCC" = "yes"])

dnl declare --enable-* args and collect ac_help strings
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug=@<:@no/minimal/yes@:>@],
                             [turn on debugging @<:@default=su_debug_default@:>@]),,
              enable_debug=su_debug_default)

AM_CONDITIONAL([DEBUG], [test x$enable_debug = xtrue])

if test "x$enable_debug" = "xyes"; then
  if test x$cflags_set != xset ; then
      case " $CFLAGS " in
      *[[\ \	]]-g[[\ \	]]*) ;;
      *) CFLAGS="$CFLAGS -ggdb3 -O0" ;;
      esac
  fi
  SU_DEBUG_FLAGS="-DG_ENABLE_DEBUG"
else
  SU_DEBUG_FLAGS="-DG_DISABLE_CAST_CHECKS"

  if test "x$enable_debug" = "xno"; then
    SU_DEBUG_FLAGS="$SU_DEBUG_FLAGS -O2 -DNDEBUG -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS"
  fi
fi

AC_SUBST(SU_EXTRA_CFLAGS)

# option to specify python interpreter to use; this just sets $PYTHON, so that
# we will fallback to reading $PYTHON if --with-python is not given, and
# python.m4 will get the expected input
AC_ARG_WITH(python,
            AS_HELP_STRING([--with-python=PATH],
                           [Path to Python interpreter; searches $PATH if only a program name is given; if not given, searches for a few standard names such as "python3" or "python2"]),
            [PYTHON="$withval"], [])
if test x"$PYTHON" = xyes; then
  AC_MSG_ERROR([--with-python option requires a path or program argument])
fi
AM_PATH_PYTHON(2.7,,PYTHON="/usr/bin/env python2.7")

dnl Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])
###LT_INIT([dlopen])
AC_SUBST([LIBTOOL_DEPS])
dnl when using libtool 2.x create libtool early, because it's used in configure
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])

dnl if we ever want/need inline, the GLib has nice macros
dnl to check which syntax to use.

# check for flavours of varargs macros
AC_MSG_CHECKING(for ISO C99 varargs macros in C)
AC_TRY_COMPILE([],[
int a(int p1, int p2, int p3);
#define call_a(...) a(1,__VA_ARGS__)
call_a(2,3);
],g_have_iso_c_varargs=yes,g_have_iso_c_varargs=no)
AC_MSG_RESULT($g_have_iso_c_varargs)

AC_MSG_CHECKING(for GNUC varargs macros)
AC_TRY_COMPILE([],[
int a(int p1, int p2, int p3);
#define call_a(params...) a(1,params)
call_a(2,3);
],g_have_gnuc_varargs=yes,g_have_gnuc_varargs=no)
AC_MSG_RESULT($g_have_gnuc_varargs)

# check for header files
AC_CHECK_HEADERS([mcheck.h stdlib.h string.h unistd.h stdarg.h syslog/syslog.h])

AC_SUBST(SU_DEBUG_FLAGS)











dnl *************************
dnl *** GIOChannel checks ***
dnl *************************

AC_MSG_CHECKING(for GIOChannel implementation)
case "$host" in
  *-*-mingw*)
    GIO=giowin32.lo
    ;;
  *)
    GIO=giounix.lo
    ;;    
esac
AC_MSG_RESULT($GIO)
AC_SUBST(GIO)

dnl **********************************
dnl *** Check for libselinux (GIO) ***
dnl **********************************
AC_ARG_ENABLE(selinux,
              AS_HELP_STRING([--disable-selinux],
                             [build without selinux support]))
msg_selinux=no
SELINUX_LIBS=
AS_IF([ test "x$enable_selinux" != "xno"], [

 AC_CHECK_LIB(selinux, is_selinux_enabled,
   [AC_CHECK_HEADERS(selinux/selinux.h,
     [AC_CHECK_LIB(selinux, lgetfilecon_raw, 
       [AC_DEFINE(HAVE_SELINUX, 1, [Define to 1 if libselinux is available])
        SELINUX_LIBS="-lselinux"
        msg_selinux=yes])
     ])
   ])
])
AC_SUBST(SELINUX_LIBS)

dnl **************************
dnl *** Checks for gtk-doc ***
dnl **************************
# gtkdocize greps for ^GTK_DOC_CHECK and parses it, so you need to have
# it on it's own line.
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([1.15], [--flavour no-tmpl])
],[
AM_CONDITIONAL([ENABLE_GTK_DOC],[false])
])

AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--enable-man],
                              [generate man pages [default=auto]])],,
              enable_man=maybe)

AS_IF([test "$enable_man" != no], [
  AC_PATH_PROG([XSLTPROC], [xsltproc])
  AS_IF([test -z "$XSLTPROC"], [
    AS_IF([test "$enable_man" = yes], [
      AC_MSG_ERROR([xsltproc is required for --enable-man])
    ])
    enable_man=no
  ])
])

# originally there was a check for DocBook DTD here, but it failed on my machine
# apparently some people have trouble with getting DocBook DTD's recognised when
# building, so I removed the check and will rather let the doc build process
# fail for now.

AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

AC_MSG_CHECKING([whether to generate man pages])
AS_IF([ test "$enable_man" != no ], [
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])

dnl ************************************
dnl *** Enable lcov coverage reports ***
dnl ************************************

AC_ARG_ENABLE(gcov,
  AS_HELP_STRING([--enable-gcov],
		 [enable coverage testing with gcov]),
  [use_gcov=$enableval], [use_gcov=no])

AS_IF([ test "x$use_gcov" = "xyes"], [
  dnl we need gcc:
  if test "$GCC" != "yes"; then
    AC_MSG_ERROR([GCC is required for --enable-gcov])
  fi

  dnl Check if ccache is being used
  AC_CHECK_PROG(SHTOOL, shtool, shtool)
  case `$SHTOOL path $CC` in
    *ccache*[)] gcc_ccache=yes;;
    *[)] gcc_ccache=no;;
  esac

  if test "$gcc_ccache" = "yes" && (test -z "$CCACHE_DISABLE" || test "$CCACHE_DISABLE" != "1"); then
    AC_MSG_ERROR([ccache must be disabled when --enable-gcov option is used. You can disable ccache by setting environment variable CCACHE_DISABLE=1.])
  fi

  ltp_version_list="1.6 1.7 1.8 1.9 1.10"
  AC_CHECK_PROG(LTP, lcov, lcov)
  AC_CHECK_PROG(LTP_GENHTML, genhtml, genhtml)

  AS_IF([ test "$LTP" ], [
    AC_CACHE_CHECK([for ltp version], su_cv_ltp_version, [
      su_cv_ltp_version=invalid
      ltp_version=`$LTP -v 2>/dev/null | $SED -e 's/^.* //'`
      for ltp_check_version in $ltp_version_list; do
        if test "$ltp_version" = "$ltp_check_version"; then
          su_cv_ltp_version="$ltp_check_version (ok)"
        fi
      done
    ])
  ], [
    ltp_msg="To enable code coverage reporting you must have one of the following LTP versions installed: $ltp_version_list"
    AC_MSG_ERROR([$ltp_msg])
  ])

  case $su_cv_ltp_version in
    ""|invalid[)]
      ltp_msg="You must have one of the following versions of LTP: $ltp_version_list (found: $ltp_version)."
      AC_MSG_ERROR([$ltp_msg])
      LTP="exit 0;"
      ;;
  esac

  if test -z "$LTP_GENHTML"; then
    AC_MSG_ERROR([Could not find genhtml from the LTP package])
  fi

  dnl Remove all optimization flags from CFLAGS
  changequote({,})
  CFLAGS=`echo "$CFLAGS" | $SED -e 's/-O[0-9]*//g'`
  changequote([,])

  dnl Add the special gcc flags
  CFLAGS="$CFLAGS -O0 -fprofile-arcs -ftest-coverage"
  LDFLAGS="$LDFLAGS -lgcov"
])

g_have_gnuc_varargs=$g_have_gnuc_varargs
g_have_iso_c_varargs=$g_have_iso_c_varargs

# Check for GDBus
PKG_CHECK_MODULES(DBUS,
     dbus-glib-1,
     [AC_DEFINE(HAVE_DBUS, 1, [Define if dbus-glib-1 is available]) have_dbus=yes],
     have_dbus=no)
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)
AM_CONDITIONAL(HAVE_DBUS, [test "x$have_dbus" = "xyes"])

AC_PATH_PROGS([GDBUS_CODEGEN], [gdbus-codegen])
AM_CONDITIONAL([HAVE_GDBUS_CODEGEN], [test x$GDBUS_CODEGEN = xgdbus-codegen ])

# Check for GLib
PKG_CHECK_MODULES(GLIB,
     glib-2.0,
     [AC_DEFINE(HAVE_GLIB, 1, [Define if GLib is available]) have_glib=yes],
     have_glib=no)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AM_CONDITIONAL(HAVE_GLIB, [test "x$have_glib" = "xyes"])

# Check for GTK+ 3.10 or more
PKG_CHECK_MODULES(GTK,
     gtk+-3.0 >= 3.10,
     [AC_DEFINE(HAVE_GTK, 1, [Define if GTK+ 3 is available]) have_gtk=yes],
     have_gtk=no)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)
AM_CONDITIONAL(HAVE_GTK, [test "x$have_gtk" = "xyes"])

AC_CHECK_PROGS([DBUS_DAEMON], [dbus-daemon])
AM_CONDITIONAL([HAVE_DBUS_DAEMON], [test x$DBUS_DAEMON = xdbus-daemon ])

SYSTEMD_REQUIRED=212
PKG_CHECK_MODULES(SYSTEMD, [ libsystemd >= $SYSTEMD_REQUIRED ],
     [AC_DEFINE(HAVE_SYSTEMD, 1, [Define if systemd is available]) have_systemd=yes],
     have_systemd=no)
AC_SUBST(SYSTEMD_CFLAGS)
AC_SUBST(SYSTEMD_LIBS)
AM_CONDITIONAL(HAVE_SYSTEMD, [test "x$have_systemd" = "xyes"])

DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])


# pkg-config systemd --variable=systemdsystemunitdir would tell me the correct
# path but then I'd need to truncate the prefix and replace it by LIBDIR #TODO
SYSTEMD_USER_DIR="$LIBDIR/systemd/user"
SYSTEMD_SYSTEM_DIR="$LIBDIR/systemd/system"
AC_SUBST(SYSTEMD_USER_DIR)
AC_SUBST(SYSTEMD_SYSTEM_DIR)
AC_DEFINE_UNQUOTED(SYSTEMD_USER_DIR, "$SYSTEMD_USER_DIR", [Where user services dir for Systemd is])
AC_DEFINE_UNQUOTED(SYSTEMD_SYSTEM_DIR, "$SYSTEMD_SYSTEM_DIR", [Where system services dir for Systemd is])


dnl Compiler flags; macro originates from systemd
dnl See https://bugzilla.gnome.org/show_bug.cgi?id=608953
AC_ARG_ENABLE(compile-warnings,
              [AS_HELP_STRING([--disable-compile-warnings],
                              [Don't use builtin compiler warnings])],,
			      enable_compile_warnings=yes)
AS_IF([test "x$enable_compile_warnings" = xyes], [
  CC_CHECK_FLAGS_APPEND([GLIB_WARN_CFLAGS], [CFLAGS], [\
   -Wall -Wstrict-prototypes -Werror=declaration-after-statement \
   -Werror=missing-prototypes -Werror=implicit-function-declaration \
   -Werror=pointer-arith -Werror=init-self -Werror=format-security \
   -Werror=format=2 -Werror=missing-include-dirs])
])
AC_SUBST(GLIB_WARN_CFLAGS)

# #FIXME im really not sure the bits below are what I want/need

# for config.h:
config_h_INCLUDES='-I$(top_builddir)'
AC_SUBST(config_h_INCLUDES)


# libsandboxutils includes, may need tweaking
libsandboxutils_INCLUDES='$(config_h_INCLUDES) -I$(top_srcdir)/lib'
AC_SUBST(libsandboxutils_INCLUDES)

# sandboxutilsd:
#   same as libsandboxutils
sandboxutilsd_INCLUDES='$(libsandboxutils_INCLUDES)'
AC_SUBST(sandboxutilsd_INCLUDES)

# lookup GLib for more examples of things to include
AC_CONFIG_FILES([
sandboxutils.pc
Makefile
lib/Makefile
server/Makefile
])
# docs/Makefile #TODO
# tests/Makefile #TODO

AC_OUTPUT
