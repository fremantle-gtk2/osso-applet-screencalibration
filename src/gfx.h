/*
 * This file is part of osso-applet-screencalibration
 *
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: Tapani Paelli <tapani.palli@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License 
 * version 2 as published by the Free Software Foundation. 
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */  


#ifndef _OSSO_APPLET_SCREENCALIBRATION_GRAPHICS_H_
#define _OSSO_APPLET_SCREENCALIBRATION_GRAPHICS_H_

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include "common.h"

#define POINT_SCR_DST_X 80
#define POINT_SCR_DST_Y 60

#define FONT "nokia sans"
#define FONT_SIZE 28

typedef struct point_struct
{
  int x;
  int y;
} hotspot;

typedef struct x_info_struct
{
  Display *dpy;
  Visual *visual;
  Colormap cmap;
  Window win;
  GC gc;

  int screen;
  int depth;
  int xres;
  int yres;

  uint active_alpha;
  uint active_angle;
  uint glow_direction;

  uint previous_angle;

  cairo_surface_t *main_surface;
  cairo_surface_t *target_active;
  cairo_surface_t *target_passive;

  int ring_radius;

  XRectangle target_extents;

  hotspot hotspots[4]; /* for each screen corner */ 

} x_info;

unsigned int
init_graphics (x_info* xinfo);

void
free_graphics (x_info *xinfo);

void
draw_rect (x_info *xinfo,
	   int x, int y, int w , int h,
	   double r, double g, double b, double a);

void
blit_active_hotspot(x_info *xinfo, int k);

void
blit_hotspots(x_info *xinfo, int active, double alpha);

void
draw_instructions (x_info *xinfo, int active, uint info);

void
draw_screen (x_info *xinfo, int active, uint info);

#endif
