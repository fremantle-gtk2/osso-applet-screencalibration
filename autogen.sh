#!/bin/sh

set -x
#glib-gettextize --copy --force
libtoolize --automake --copy
#intltoolize --copy --force --automake
aclocal
autoconf
autoheader
automake --add-missing --foreign
