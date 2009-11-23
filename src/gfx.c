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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include "gfx.h"
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>

#define PATH_BACKSPACE "/usr/share/icons/hicolor/32x32/hildon/general_backspace.png"

static uint rotation = 0;

/* Apply calibration values by setting touchscreen device property 
 * cal_evdev contains: min x, max x, min y, max y */
void
set_calibration_prop (x_info *xinfo, cal_evdev *p)
{
  int i;
  char *data = calloc (4,4);
  if (!xinfo || !p)
    return;

  Atom prop = XInternAtom(xinfo->dpy, "Evdev Axis Calibration", False);

  for (i = 0; i < 4; i++) {
	  *(((int32_t*)data) + i) = p->params[i];
	}

  XChangeDeviceProperty(xinfo->dpy, xinfo->pointer, prop, XA_INTEGER, 32,
						PropModeReplace, (unsigned char*)data, 4);

  XCloseDevice(xinfo->dpy, xinfo->pointer);

  free (data);
}

static void
change_active_target (x_info *xinfo)
{
  struct tm now;

  time_t x = time(NULL);
  localtime_r (&x, &now);

  if (now.tm_mon  == 11 &&
      now.tm_mday == 24)
  {
    rotation = 1;
  }
}

static int
init_input (x_info* xinfo)
{
  XDeviceInfo *info;
  int          ndevices,
               i;

  if (!xinfo)
    return 0;

  xinfo->pointer = NULL;
  xinfo->keyboard = NULL;

  XQueryInputVersion(xinfo->dpy, XI_2_Major, XI_2_Minor);
  
  info = XListInputDevices(xinfo->dpy, &ndevices);

  for (i = 0; i < ndevices; i++) {
    
	XDeviceInfo* current = &info[i];
    
	switch(current->use) {
      case IsXExtensionPointer:
      /* physical device for touchscreen */
	    /* open touchscreen */
	    if (!xinfo->pointer) {
		    xinfo->pointer = XOpenDevice(xinfo->dpy, current->id);
	    }
	    break;

	  case IsXExtensionKeyboard:
      /* physical device for keyboard */
	    break;
      case IsXExtensionDevice:
      /* slave device */
        break;
      case IsXPointer:
      /* master pointer */
        break;
      case IsXKeyboard:
      /* master keyboard */
        break;
        }
    }

    XFreeDeviceList(info);

	/* register for other events */
    XSelectInput(xinfo->dpy, xinfo->win,
	       StructureNotifyMask | VisibilityChangeMask |
#ifndef ARM_TARGET
	       /* mouse buttons are for x86 debugging */
	       ButtonPressMask | ButtonReleaseMask |
#endif
	       ExposureMask | LeaveWindowMask );
 
	return 1;
}

unsigned int
init_graphics (x_info *xinfo)
{
  XSetWindowAttributes wa;
  Atom state_atom, fullscreen_atom, dnd_atom, name_atom;

  memset (xinfo, 0, sizeof(x_info));

  xinfo->dpy = XOpenDisplay (NULL);
  if (!xinfo->dpy)
    {
      xinfo->dpy = XOpenDisplay (":0");
      if(!xinfo->dpy)
	{
	  return 0;
	}
    }

  xinfo->screen = DefaultScreen(xinfo->dpy);
  xinfo->depth  = DefaultDepth(xinfo->dpy, xinfo->screen);
  xinfo->cmap   = DefaultColormap(xinfo->dpy, xinfo->screen);
  xinfo->visual = DefaultVisual(xinfo->dpy, xinfo->screen);
  xinfo->xres   = ScreenOfDisplay(xinfo->dpy, DefaultScreen(xinfo->dpy))->width;
  xinfo->yres   = ScreenOfDisplay(xinfo->dpy, DefaultScreen(xinfo->dpy))->height;
  xinfo->gc     = DefaultGC(xinfo->dpy, xinfo->screen);

  wa.background_pixel  = WhitePixel(xinfo->dpy, xinfo->screen);

  xinfo->win = XCreateWindow (xinfo->dpy,
                              DefaultRootWindow(xinfo->dpy),
                              0,
                              0,
                              xinfo->xres,
                              xinfo->yres,
                              0,
                              xinfo->depth,
                              InputOutput,
                              xinfo->visual,
                              CWBackPixel, &wa);

  if (!xinfo->win)
    {
      return 0;
    }

  /* set _WM_NAME to empty string */
  char empty = '\0';
  name_atom       = XInternAtom(xinfo->dpy, "_NET_WM_NAME", False);
  XChangeProperty(xinfo->dpy, xinfo->win, name_atom, XA_STRING, 8,
                  PropModeReplace, (const unsigned char*)&empty, 1);

  /* set WM_CLASS */
  XClassHint h;
  h.res_name = "tscalibrate";
  h.res_class = "Controlpanel";
  XSetClassHint (xinfo->dpy, xinfo->win, &h);

  /* set fullscreen atom */
  state_atom      = XInternAtom(xinfo->dpy, "_NET_WM_STATE", False);
  fullscreen_atom = XInternAtom(xinfo->dpy, "_NET_WM_STATE_FULLSCREEN", False);

  XChangeProperty(xinfo->dpy, xinfo->win, state_atom, XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)&fullscreen_atom, 1);

  /* set DoNotDisturb flag */
  unsigned int set = 1;
  dnd_atom       = XInternAtom(xinfo->dpy, "_HILDON_DO_NOT_DISTURB", False);
  XChangeProperty(xinfo->dpy, xinfo->win, dnd_atom, XA_INTEGER, 32,
                  PropModeReplace, (const unsigned char*)&set, 1);

  if (!init_input (xinfo))
	 return 0;

  XFlush (xinfo->dpy);
  XSync (xinfo->dpy, 0);

  xinfo->main_surface = cairo_xlib_surface_create (xinfo->dpy,
						   xinfo->win,
						   xinfo->visual,
						   xinfo->xres,
						   xinfo->yres);
      
      char path[256];

      /* FIXME: Temporary solution until themes deliver the images */
      snprintf (path, 256, "/usr/share/pixmaps/qgn_plat_cpa_screen_calib_target_active.png");
      xinfo->target_active = cairo_image_surface_create_from_png (path);

      snprintf (path, 256, "/usr/share/pixmaps/qgn_plat_cpa_screen_calib_target_passive.png");
      xinfo->target_passive = cairo_image_surface_create_from_png (path);

  if (!xinfo->target_active || !xinfo->target_passive)
    {
      ERROR ("error loading images\n");
      abort();
    }

  change_active_target (xinfo);

  xinfo->target_extents.width  = cairo_image_surface_get_width  (xinfo->target_active);
  xinfo->target_extents.height = cairo_image_surface_get_height (xinfo->target_active);
  xinfo->ring_radius = xinfo->target_extents.width / 2;

  XMapRaised(xinfo->dpy, xinfo->win);

  /*
   * init hotspots for each screen corner
   */

  xinfo->hotspots[0].x = POINT_SCR_DST_X;
  xinfo->hotspots[0].y = POINT_SCR_DST_Y;
  xinfo->hotspots[1].x = xinfo->xres - POINT_SCR_DST_X;
  xinfo->hotspots[1].y = POINT_SCR_DST_Y;
  xinfo->hotspots[2].x = xinfo->xres - POINT_SCR_DST_X;
  xinfo->hotspots[2].y = xinfo->yres - POINT_SCR_DST_Y;
  xinfo->hotspots[3].x = POINT_SCR_DST_X;
  xinfo->hotspots[3].y = xinfo->yres - POINT_SCR_DST_Y;

  return 1;
}

/*
 * free allocated graphics data
 */
void
free_graphics (x_info *xinfo)
{
  if (!xinfo)
    return;

  cairo_surface_destroy(xinfo->main_surface);
  cairo_surface_destroy(xinfo->target_active);
  cairo_surface_destroy(xinfo->target_passive);
}


static void
draw_text (x_info *xinfo, int x, int y, const char *font, const char*txt)
{
  cairo_t *c;
  c = cairo_create (xinfo->main_surface);
  cairo_select_font_face(c, font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(c, FONT_SIZE);
  cairo_move_to(c, x, y);
  cairo_show_text(c, txt);
  cairo_destroy(c);
}

static void
draw_text_center_color (x_info *xinfo, int y, const char*txt,
			double red, double green, double blue, double alpha)
{
  cairo_text_extents_t textext;
  cairo_t *c;

  c = cairo_create (xinfo->main_surface);
  cairo_select_font_face(c, "nokia sans", CAIRO_FONT_SLANT_NORMAL,
			 CAIRO_FONT_WEIGHT_NORMAL);

  cairo_set_font_size(c, FONT_SIZE*4);
  cairo_text_extents(c, txt, &textext);

  cairo_move_to(c, (xinfo->xres - textext.width)/2, y);
  cairo_rotate(c, 6 * M_PI / 180);

  cairo_set_source_rgba (c, red, green, blue, alpha);
  cairo_show_text(c, txt);
  cairo_destroy(c);
}


static void
draw_text_center (x_info *xinfo, int y, const char*txt)
{
  cairo_text_extents_t textext;
  cairo_t *c;

  c = cairo_create (xinfo->main_surface);
  cairo_select_font_face(c, "nokia sans", CAIRO_FONT_SLANT_NORMAL,
			 CAIRO_FONT_WEIGHT_NORMAL);

  cairo_set_font_size(c, FONT_SIZE);
  cairo_text_extents(c, txt, &textext);
  cairo_move_to(c, (xinfo->xres - textext.width)/2, y);
  cairo_show_text(c, txt);
  cairo_destroy(c);
}


static void
get_text_extents (x_info *xinfo, char *txt, char *font, int*w, int*h)
{
  cairo_text_extents_t textext;
  cairo_t *c;

  c = cairo_create (xinfo->main_surface);
  cairo_select_font_face(c, font, CAIRO_FONT_SLANT_NORMAL,
			 CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(c, FONT_SIZE);
  cairo_text_extents(c, txt, &textext);

  *w = textext.width;
  *h = textext.height;

  cairo_destroy(c);
}


void
draw_rect (x_info *xinfo,
           int x, int y, int w , int h,
	   double r, double g, double b, double a)

{
  cairo_t *c;
  c = cairo_create (xinfo->main_surface);
  cairo_rectangle (c, x, y, w, h);
  cairo_set_source_rgba (c, r, g, b, a);
  cairo_fill(c);
  cairo_destroy(c);
}


static void
blit_surface (cairo_t *c,
	      cairo_surface_t *surface,
	      int x, int y,
	      double alpha)
{
  cairo_rectangle (c, x, y,
		   cairo_image_surface_get_width  (surface),
		   cairo_image_surface_get_height (surface));
  cairo_clip (c);

  cairo_move_to (c, x, y);
  cairo_set_source_surface (c, surface, x - 0.5, y - 0.5);
  cairo_paint_with_alpha (c, alpha);

  cairo_reset_clip (c);
}


static void
blit_roto_surface(x_info *xinfo,
		  cairo_surface_t *surface,
		  int x, int y, int angle, uint glow_arc)
{
  cairo_t *c;
  int w, h;

  w = cairo_image_surface_get_width  (surface);
  h = cairo_image_surface_get_height (surface);

  c = cairo_create (xinfo->main_surface);

  if (rotation)
    {
      cairo_translate(c, x + (w / 2), y + (h / 2));
      cairo_rotate(c, angle * M_PI / 180);
      cairo_translate(c, -x - (w / 2), -y - (h / 2));
    }

  cairo_rectangle(c, x, y,
		  cairo_image_surface_get_width  (surface),
		  cairo_image_surface_get_height (surface));
  cairo_clip(c);

  cairo_set_source_surface (c, surface, x - 0.5, y - 0.5);
  cairo_paint(c);

#define ARC_SIZE 12
  if (glow_arc)
    {
      cairo_reset_clip(c);

      cairo_set_line_width (c, 0.6 * cairo_get_line_width (c));
      cairo_arc (c, x+w/2, y+h/2, ARC_SIZE, 0, 2 * M_PI);
      cairo_set_source_rgba (c, 1.0, 0, 0, 1.0/xinfo->active_alpha);

      cairo_stroke(c);
    }

  //  cairo_show_page(c);
  cairo_destroy(c);
}

void
blit_active_hotspot(x_info *xinfo, int k)
{
  blit_roto_surface (xinfo,
		     xinfo->target_active,
		     xinfo->hotspots[k].x - xinfo->ring_radius,
		     xinfo->hotspots[k].y - xinfo->ring_radius,
		     xinfo->active_angle++, 1);


  if (rotation)
    {
      if (k>0 && xinfo->previous_angle%90 != 0)
	{
	  blit_roto_surface (xinfo,
			     xinfo->target_passive,
			     xinfo->hotspots[k-1].x - xinfo->ring_radius,
			     xinfo->hotspots[k-1].y - xinfo->ring_radius,
			     xinfo->previous_angle++, 0);
	  if (xinfo->previous_angle%90 == 0)
	    xinfo->previous_angle=0;
	}
    }

  if (xinfo->glow_direction)
    {
      xinfo->active_alpha -= 1;

      if (xinfo->active_alpha <= 0)
	xinfo->glow_direction=0;
    }
  else
    {
      xinfo->active_alpha += 1;

      if (xinfo->active_alpha >= 10)
	xinfo->glow_direction=1;
    }
}


void
blit_hotspots (x_info *xinfo, int active, double alpha)
{
  int xx, k, w, h;
  char buffer[4];
  cairo_t *c;

  c = cairo_create (xinfo->main_surface);

  for (k=0; k<4; k++)
    {
	if (k != active)
	  {
	    if (rotation && k == active-1)
	      continue;
	    blit_surface (c,
			  xinfo->target_passive,
			  xinfo->hotspots[k].x - xinfo->ring_radius,
			  xinfo->hotspots[k].y - xinfo->ring_radius,
			  alpha);
	  }

      snprintf(buffer, 4, "%d", k+1);

      if (xinfo->hotspots[k].x > xinfo->xres/2)
	{
	  xx = (xinfo->hotspots[k].x - xinfo->ring_radius) - 28;
	}
      else
	{
	  xx = (xinfo->hotspots[k].x - xinfo->ring_radius)
	    + cairo_image_surface_get_width (xinfo->target_active) + 12;
	}

      get_text_extents (xinfo, buffer, FONT, &w, &h);

      draw_text (xinfo, xx,
		 (xinfo->hotspots[k].y - xinfo->ring_radius)
		 + cairo_image_surface_get_width (xinfo->target_active) / 2 + h/2,
		 FONT, buffer);

    }

  cairo_destroy(c);
}

void
draw_instructions (x_info *xinfo, int active, uint info)
{
  char buffer[256];
  char *tmp, *tail;
  int w, h;

  get_text_extents (xinfo, _("scca_fi_calibrate"), FONT, &w, &h);

  draw_rect (xinfo, 0, xinfo->yres/2 - 3*h, xinfo->xres, 7*h, 1.0, 1.0, 1.0, 1.0);

  if (info != TAP_COMPLETE)
    draw_text_center(xinfo, xinfo->yres/2 - 2*h, _("scca_fi_calibrate"));

  switch (info)
    {
    case TAP_CLOSER :
      snprintf(buffer, 256, _("scca_fi_tap_closer"), active + 1);
      break;
    case TAP_NEXT_TARGET :
      snprintf(buffer, 256, _("scca_fi_tap_to"), active + 1);
      break;
    case TAP_COMPLETE :
      snprintf(buffer, 256, _("scca_ib_calib_success"));
      break;
    }

  draw_text_center(xinfo, xinfo->yres/2, buffer);

   if (info == TAP_COMPLETE)
      return;

  /*
   * text with <esc> symbol
   */

  snprintf(buffer, 256, _("scca_fi_press_esc_cancel"));

  tmp = strstr(buffer,"<esc");
  if (tmp)
    {
      int w1, w2, w3, xx;
	  unsigned char term[3] = {' ', ' ', '\0'};
	  cairo_surface_t* back = cairo_image_surface_create_from_png(PATH_BACKSPACE);
	  cairo_status_t status = cairo_surface_status (back);
      if (status) {
	     ERROR ("Error reading %s\n", PATH_BACKSPACE);
	  }
	  cairo_t* ct_back = cairo_create (xinfo->main_surface);
      
	  tail = strstr(buffer,">");
      memcpy (tmp, &term, 3*sizeof(char));

      get_text_extents (xinfo, buffer, FONT, &w1, &h);
	  w2 = cairo_image_surface_get_width (back);
      get_text_extents (xinfo, tail, FONT, &w3, &h);
	  h = cairo_image_surface_get_height (back);

      w1 += 10;
      w2 += 10;

      xx = (xinfo->xres - (w1+w2+w3))/2;

	  draw_text (xinfo, xx,       xinfo->yres/2 + h*2, FONT, buffer);

	  /* draw the icon for cancel */
      cairo_set_source_surface (ct_back, back, xx+w1, xinfo->yres/2 + h);
	  cairo_paint (ct_back);
	  cairo_destroy (ct_back);
      draw_text (xinfo, xx+w1+w2, xinfo->yres/2 + h*2, FONT, ++tail);
    }
}


void
draw_screen (x_info *xinfo, int active, uint info)
{
  XSync(xinfo->dpy, 1);

  draw_instructions (xinfo, active, info);
  XSync(xinfo->dpy, 1);

  blit_hotspots (xinfo, active, 1.0);
  XSync(xinfo->dpy, 1);
}

