#
# This file is part of osso-applet-screencalibration
#
# Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).
#
# Contact: Tapani Paelli <tapani.palli@nokia.com>
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
#
# ----------------------------------------------------------
#
# Makefile for touchscreen applet
#
#  ts_cfg          - executable for configuration writing
#  tscalib.so      - dynamic library for control panel
#  tscalibrate     - calibration program
#  tscalib.desktop - configuration file for control panel
#
#  launcher is depending on gtk because of the current API
#


SOURCES = \
	  main.c        \
	  gfx.c         \
	  common.h      \
	  gfx.h


AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src
CFLAGS = @CFLAGS@ @DEFS@ $(AM_CPPFLAGS)
DEPS = @CAIRO_LIBS@ @CAIRO_CFLAGS@ @XI_CFLAGS@ @XI_LIBS@ $(CFLAGS)
LAUNCHER_DEPS = @GTK_CFLAGS@ @GTK_LIBS@ @LIBOSSO_CFLAGS@ @LIBOSSO_LIBS@ $(CFLAGS)


DESTDIR = @DESTDIR@
BINARY=tscalibrate
CC = gcc -g -O2

all :
	$(CC) -o $(BINARY) $(SOURCES) $(ARM_OPS) $(DEPS) -lm
	$(CC) -D_GNU_SOURCE -shared -fPIC -o tscalib.so launch.c $(LAUNCHER_DEPS) 

install :
	install -m 755 $(BINARY) $(DESTDIR)/usr/bin
	install -m 644 tscalib.so $(DESTDIR)/usr/lib/hildon-control-panel
clean :
	rm -f @PROGRAM@ tscalib.so *~ core*
