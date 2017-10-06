AC_PREREQ(2.59)

AC_INIT(osso-applet-screencalibration, 1.12, peter.aradi@blumsoft.eu)
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

PKG_PROG_PKG_CONFIG

if test x`dpkg --print-architecture` = xarmel; then \
	AC_DEFINE(ARM_TARGET, 1, [Description]) \
	else \
	AC_DEFINE(X86_TARGET, 1, [Description]) \
fi;

AC_ARG_ENABLE(debug, [AC_HELP_STRING([--enable-debug],[Debug (default=no)])],
                            [hildon_use_debug=yes],[hildon_use_debug=no])

if test "x${hildon_use_debug}" = "xyes"
then
   CFLAGS="$CFLAGS -ggdb -O0 -DDEBUG"
else
   CFLAGS="$CFLAGS -O2"
fi

AC_SUBST(CFLAGS)

# dependencies
PKG_CHECK_MODULES(CAIRO, cairo)
AC_SUBST(CAIRO_LIBS)
AC_SUBST(CAIRO_CFLAGS)

PKG_CHECK_MODULES(XI, x11 xi)
AC_SUBST(XI_LIBS)
AC_SUBST(XI_CFLAGS)

PKG_CHECK_MODULES(GTK, gtk+-2.0)
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)

PKG_CHECK_MODULES(LIBOSSO, libosso)
AC_SUBST(LIBOSSO_LIBS)
AC_SUBST(LIBOSSO_CFLAGS)

PROGRAM=osso-applet-screencalibration
AC_SUBST(PROGRAM)

AC_CONFIG_FILES([
Makefile \
src/Makefile \
etc/Makefile
])

AC_OUTPUT
