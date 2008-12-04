
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

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#ifdef ARM_TARGET
#include <X11/extensions/Xsp.h>
#endif

#include "calibration.h"
#include "common.h"
#include "gfx.h"
#include <gtk/gtk.h>

#define DEFAULT_CALIBRATION_FILE "/etc/pointercal.default"
#define HOTSPOT_AMOUNT 3

#define ANIM_TIMEOUT 30000

static x_info xinfo;
static calibration cal;

#ifdef ARM_TARGET
static calibration defcal;
#endif

/*
 * called when program exits, sets rawmode off
 */
static void
closing_procedure (void)
{
#ifdef ARM_TARGET
  XSPSetTSRawMode(xinfo.dpy, False);
#endif
  XCloseDisplay(xinfo.dpy);
}



#ifdef ARM_TARGET
/*****************************************************************/
/*
 * rawmode needs to be turned off in SIGSEGV etc.
 */
static void
signal_handler (int sig)
{
  XSPSetTSRawMode(xinfo.dpy, False);
}

/*
 * read 'factory default' values
 */
static uint
read_default_calibration (void)
{
  FILE *in;
  uint items;
  in = fopen (DEFAULT_CALIBRATION_FILE, "r");

  if (in)
    {
      items = fscanf (in, "%d %d %d %d %d %d %d",
		      &defcal.a[1], &defcal.a[2], &defcal.a[0],
		      &defcal.a[4], &defcal.a[5], &defcal.a[3],
		      &defcal.a[6]);
      fclose (in);
      if (items != 7)
	{
	  return 0;
	}

      return 1;
    }
  ERROR ("could not open default calibration file\n");
  return 0;
}


/*
 * set 'factory default' values
 */
static uint
reset_calibration (x_info *xinfo)
{
  if(!XSPSetTSCalibration(xinfo->dpy,
			  defcal.a[0], defcal.a[1], defcal.a[2],
			  defcal.a[3], defcal.a[4], defcal.a[5],
			  defcal.a[6]))
    {
      return 0;
    }
  return 1;
}


/*
 * Translate device coordinates to screen coordinates
 * This is needed only for making sure that taps are not totally out
 * of range, which would render the touchscreen unusable after
 * calibration. Use the factory default calibration matrix.
 */
static void
translate (x_info* xinfo, int *x, int *y)
{
  *x = (defcal.a[1] * *x + defcal.a[2] * *y + defcal.a[0]) / defcal.a[6];
  *y = (defcal.a[4] * *x + defcal.a[5] * *y + defcal.a[3]) / defcal.a[6];
}

/*
 * returns distance between given coordinates
 * and given XPoint.
 */
static uint distance (int x1, int y1, int x2, int y2)
{
  return (abs(x2 - x1) + abs(y2 - y1));
}

/*
 * sorting functions for qsort
 */

static int sort_by_x (const void* a, const void *b)
{
  return (((XSPRawTouchscreenEvent *)a)->x - ((XSPRawTouchscreenEvent *)b)->x);
}
static int sort_by_y (const void* a, const void *b)
{
  return (((XSPRawTouchscreenEvent *)a)->y - ((XSPRawTouchscreenEvent *)b)->y);
}
/*****************************************************************/
#endif

static int
timediff (struct timeval *starttime,
          struct timeval *endtime)
{
  endtime->tv_usec -= starttime->tv_usec;
  endtime->tv_sec  -= starttime->tv_sec;

  if (endtime->tv_usec < 0)
    {
      endtime->tv_usec += 1000000;
      endtime->tv_sec--;
    }
  return (endtime->tv_sec*1000 + endtime->tv_usec/1000);
}

static void
fade_away_loop (void)
{
  struct timeval now, start;
  double alpha=0;
  int k;

  gettimeofday (&start, NULL);
  for (;;)
  {
    usleep (ANIM_TIMEOUT);
    gettimeofday (&now, NULL);
    if (timediff (&start, &now) > 2500)
    {
      break;
    }
  }
}

static Window *
query_tree (x_info* xinfo, int w,  int *num_children)
{
  Window root_win, parent_win;
  Window *child_list;
  Status status;

  status = XQueryTree (xinfo->dpy,
		       w,
		       &root_win, 
		       &parent_win, 
		       &child_list,
		       num_children);

  if (status == 0 || num_children == 0 || child_list == NULL)
    return NULL;
  return child_list;
}

static Atom
get_wintype_prop (Display *dpy, Window w)
{
  Atom actual;
  int format;
  unsigned long n, left;
  unsigned char *data;
  Atom winTypeAtom   = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
  Atom winNormalAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);

  int result = XGetWindowProperty (dpy, w, winTypeAtom, 0L, 1L, False,
                                   XA_ATOM, &actual, &format,
                                   &n, &left, &data);

  if (result == Success && data != None)
    {
      Atom a;
      memcpy (&a, data, sizeof (Atom));
      XFree ( (void *) data);
      return a;
    }
  return winNormalAtom;
}


/*
 * read events & draw graphics
 */
static void
calibration_event_loop (void)
{
  XEvent ev;
  int event_amount;
  long key_pressed;
  int ACTIVE_HOTSPOT = 0;

#ifdef ARM_TARGET
#define MAX_SAMPLES 256
  XSPRawTouchscreenEvent samp[MAX_SAMPLES];

  int xsp_event_base = -1;
  int xsp_error_base = -1;
  int xsp_major      = -1;
  int xsp_minor      = -1;

  XSPQueryExtension(xinfo.dpy,
                    &xsp_event_base,
                    &xsp_error_base,
                    &xsp_major,
                    &xsp_minor);
  if (xsp_event_base < 0)
    {
      ERROR ("XSP extension not found\n");
      goto exit;
    }

  XSPSetTSRawMode(xinfo.dpy, True);
#endif

  draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_NEXT_TARGET);
  blit_active_hotspot (&xinfo, ACTIVE_HOTSPOT);

  /* swallow events */
  XFlush (xinfo.dpy);
  XSync (xinfo.dpy, 1);

  usleep (250000);

  for (;;)
    {
      event_amount = XEventsQueued (xinfo.dpy, QueuedAfterReading);

      if (event_amount < 1)
	{
	  blit_active_hotspot (&xinfo, ACTIVE_HOTSPOT);
	  XFlush(xinfo.dpy);
	  usleep (ANIM_TIMEOUT);
	  continue;
	}

      XNextEvent (xinfo.dpy, &ev);

#ifdef ARM_TARGET
      /* touchscreen event */
      if (ev.type == xsp_event_base)
	{
	  int index   = 0;
	  int middle  = 0;
	  int raw_x   = 0;
	  int raw_y   = 0;
	  int trans_x = 0;
	  int trans_y = 0;

	  /* read all touchscreen events from queue */
	  while (((ev.type == xsp_event_base) && (index < event_amount))&&(index<MAX_SAMPLES))
	    {
	      memcpy(&samp[index], &ev, sizeof(XSPRawTouchscreenEvent));
	      if (samp[index].x > 0 && samp[index].y > 0)
		{
		  index++;
		}
	      if (event_amount > 1)
		{
		  XNextEvent(xinfo.dpy, &ev);
		  event_amount--;
		}
	    }

	  middle = index/2;

	  qsort(samp, index, sizeof(XSPRawTouchscreenEvent), sort_by_x);
	  if (index & 1)
	    {
	      raw_x = samp[middle].x;
	    }
	  else
	    {
	      raw_x = (samp[middle-1].x + samp[middle].x) / 2;
	    }

	  qsort(samp, index, sizeof(XSPRawTouchscreenEvent), sort_by_y);
	  if (index & 1)
	    {
	      raw_y = samp[middle].y;
	    }
	  else
	    {
	      raw_y = (samp[middle-1].y + samp[middle].y) / 2;
	    }

	  trans_x = raw_x;
	  trans_y = raw_y;

	  translate (&xinfo, &trans_x, &trans_y);

#define POINT_MAX_DISTANCE 64
	  if (distance (trans_x, trans_y,
			xinfo.hotspots[ACTIVE_HOTSPOT].x,
			xinfo.hotspots[ACTIVE_HOTSPOT].y) < POINT_MAX_DISTANCE)
	    {
	      cal.x[ACTIVE_HOTSPOT] = raw_x;
	      cal.y[ACTIVE_HOTSPOT] = raw_y;

	      ACTIVE_HOTSPOT++;

	      xinfo.previous_angle = xinfo.active_angle;
	      xinfo.active_angle = 0;

	      if (ACTIVE_HOTSPOT > HOTSPOT_AMOUNT)
		{
		  /* calculate center point */
		  cal.x[4] = cal.x[0] - ((cal.x[0] - cal.x[1])/2);
		  cal.y[4] = cal.y[0] - ((cal.y[0] - cal.y[3])/2);

		  if (perform_calibration(&cal))
		    {
		      XSPSetTSCalibration(xinfo.dpy,
					  cal.a[0], cal.a[1], cal.a[2],
					  cal.a[3], cal.a[4], cal.a[5],
					  cal.a[6]);
		    }

		  ACTIVE_HOTSPOT = 42;

		  draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_COMPLETE);

		  XFlush (xinfo.dpy);
		  XSync (xinfo.dpy, 1);

		  usleep (2500000);

		  break;
		}

	      /* neeext! */
	      draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_NEXT_TARGET);

	      XFlush (xinfo.dpy);
	      usleep (500000);
	      XSync (xinfo.dpy, 1);
	    }
	  else
	    {
	      draw_instructions (&xinfo, ACTIVE_HOTSPOT, TAP_CLOSER);
	    }


	  continue;
	} /* if it was touchscreen event*/
#endif

      /*
       * other events ...
       */

      switch (ev.type)
      {
      case VisibilityNotify :
	/* another window is on top of us -> exit */
	if (((XVisibilityEvent *)&ev)->state != VisibilityUnobscured)
	{
	  int amount = 0, camount = 0, i = 0, j = 0;
	  Window *list = query_tree (&xinfo, DefaultRootWindow (xinfo.dpy), &amount);
	  Atom winNotificationAtom = XInternAtom (xinfo.dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
	  unsigned int banner=0;

	  for (i = 0; i < amount; i++) 
	  {
	    Window *child_list = query_tree (&xinfo, list[amount-1], &camount);

	    for (j = 0; j < camount; j++) 
	    {
	      if (get_wintype_prop (xinfo.dpy, child_list[j]) == winNotificationAtom)
	      {
		banner = 1;
	      }
	    }
	    if (child_list)
	      XFree (child_list);
	  }
	  if (list)
	    XFree (list);

	  if (!banner)
	    goto exit;
	}
	break;

      case KeyPress :
	key_pressed = XLookupKeysym ((XKeyEvent *)&ev, 0);

	if ((XKeysymToString(key_pressed) == NULL) ||
	    (key_pressed == XK_Execute || key_pressed == XK_F5))
	{
	  goto exit;
	}
	else if (key_pressed == XK_Escape)
	{
	  /*
	   * user exit might be handled differently
	   */
	  goto exit;
	}
	break;

#ifndef ARM_TARGET
      case ButtonRelease :
	ACTIVE_HOTSPOT++;

	xinfo.previous_angle = xinfo.active_angle;
	xinfo.active_angle = 0;

	if (ACTIVE_HOTSPOT > HOTSPOT_AMOUNT)
	{
	  ACTIVE_HOTSPOT = 42;
	  draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_COMPLETE);
	  XFlush (xinfo.dpy);
	  XSync (xinfo.dpy, 1);
	  usleep (2500000);
	  goto exit;
	}
	draw_screen (&xinfo, ACTIVE_HOTSPOT, 0);
	XFlush(xinfo.dpy);
	break;
#endif

      case Expose:
	if (ev.xexpose.count == 0)
	{
	  /*
	   * fullscreen draw
	   */
	  XClearWindow(xinfo.dpy, xinfo.win);
	  draw_screen (&xinfo, ACTIVE_HOTSPOT, 0);
	  XFlush(xinfo.dpy);
	}
	break;

      case ClientMessage:
	goto exit;
	break;
      }
    }

 exit:
  return;
}


int main (int argc, char **argv)
{

  /*
   * setup localization
   */

  setlocale (LC_ALL, "");
  bindtextdomain ("osso-applet-screencalibration", "/usr/share/locale");
  bind_textdomain_codeset("osso-applet-screencalibration", "UTF-8");
  textdomain("osso-applet-screencalibration");

  gtk_init (&argc, &argv);
  
  if (!init_graphics (&xinfo))
  {
    ERROR("graphics initialization failed\n");
    goto away;
  }

  /*
   * reset 'factory default' values from command line
   */
#ifdef ARM_TARGET
  if (argc > 1)
  {
    if(!strcmp("reset", argv[1]))
    {
      reset_calibration(&xinfo);
      goto away;
    }
  }

  if (!read_default_calibration())
  {
    goto away;
  }
#endif

  if (XGrabKeyboard(xinfo.dpy,
                    xinfo.win,
                    False,
                    GrabModeAsync,
                    GrabModeAsync,
                    CurrentTime) == AlreadyGrabbed)
    goto away;

  XGrabPointer (xinfo.dpy,
		xinfo.win,
		False,
		ButtonReleaseMask,
		GrabModeAsync,
		GrabModeAsync,
		xinfo.win,
		None,
		CurrentTime);

  /* copy hotspot locations to calibration struct */

  cal.xfb[0] = xinfo.hotspots[0].x;  cal.yfb[0] = xinfo.hotspots[0].y;
  cal.xfb[1] = xinfo.hotspots[1].x;  cal.yfb[1] = xinfo.hotspots[1].y;
  cal.xfb[2] = xinfo.hotspots[2].x;  cal.yfb[2] = xinfo.hotspots[2].y;
  cal.xfb[3] = xinfo.hotspots[3].x;  cal.yfb[3] = xinfo.hotspots[3].y;
  cal.xfb[4] = xinfo.xres/2;  cal.yfb[4] = xinfo.yres/2;

  XFlush(xinfo.dpy);
  XSync(xinfo.dpy, 0);

  if (atexit(closing_procedure) != 0)
  {
    goto away;
  }

  /*
   * catch signals so that we can turn rawmode off in error situations
   */
#ifdef ARM_TARGET
  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGSEGV, signal_handler);
#endif

  calibration_event_loop();

  free_graphics (&xinfo);

  away :
    return 0;
}

