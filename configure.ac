AC_PREREQ(2.59)

AC_INIT(osso-applet-screencalibration, 1.12, peter.aradi@blumsoft.eu)
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC

AM_PROG_LIBTOOL

PKG_PROG_PKG_CONFIG

AC_ARG_ENABLE(debug, [AC_HELP_STRING([--enable-debug],[Debug (default=no)])],
                            [hildon_use_debug=yes],[hildon_use_debug=no])

if test "x${hildon_use_debug}" = "xyes"
then
   CFLAGS="$CFLAGS -DDEBUG -DTS_DEBUG"
else
   CFLAGS="$CFLAGS -Wall"
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

hildoncontrolpanellibdir=`pkg-config hildon-control-panel --variable=pluginlibdir`
AC_SUBST(hildoncontrolpanellibdir)

hildoncontrolpaneldatadir=`pkg-config hildon-control-panel --variable=plugindesktopentrydir`
AC_SUBST(hildoncontrolpaneldatadir)

AC_CONFIG_FILES([
Makefile \
src/Makefile \
etc/Makefile
])

AC_OUTPUT
