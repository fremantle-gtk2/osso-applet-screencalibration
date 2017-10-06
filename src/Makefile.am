#
# This file is part of osso-applet-screencalibration
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License 
# version 2 as published by the Free Software Foundation. 
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA

bin_PROGRAMS = tscalibrate
tscalibrate_SOURCES = \
	  main.c        \
	  gfx.c         \
	  common.h      \
	  gfx.h


tscalibrate_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src $(CAIRO_CFLAGS) \
		$(XI_CFLAGS) $(ARM_OPS)
tscalibrate_LDADD = $(CAIRO_LIBS) $(XI_LIBS)

hildon_control_panel_la_LTLIBRARIES = tscalib.la
hildon_control_panel_ladir = $(hildoncontrolpanellibdir)

tscalib_la_SOURCES = launch.c
tscalib_la_CFLAGS = $(GTK_CFLAGS) $(LIBOSSO_CFLAGS)
tscalib_la_LDFLAGS = -module -avoid-version -shared
tscalib_la_LIBADD = $(GTK_LIBS) $(LIBOSSO_LIBS)

install-data-hook:
	@(find $(DESTDIR)$(hildon_control_panel_ladir) -type f -name \*.la -delete)

MAINTAINERCLEANFILES = Makefile.in
