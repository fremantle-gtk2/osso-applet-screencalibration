
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
#include <X11/extensions/XInput.h>

#include "calibration.h"
#include "common.h"
#include "gfx.h"
#include <tslib.h>

#define HOTSPOT_AMOUNT 3

#define ANIM_TIMEOUT 30000

static x_info xinfo;
static calibration cal;

#ifdef ARM_TARGET
#define DEFAULT_CAL_PARAMS "14114 18 -2825064 34 -8765 32972906 65536"
#define XCONF "/etc/xorg.conf-RX-51.default"
#define CANCEL 22
static struct tsdev *ts;
static calibration defcal;
#endif

/*
 * called when program exits, sets rawmode off
 */
static void
closing_procedure (void)
{
  XCloseDisplay(xinfo.dpy);
}

#ifdef ARM_TARGET
/*****************************************************************/
/*
 * read 'factory default' parameters for coordinate translation
 */
static uint
read_default_calibration (void)
{
  uint items;
  items = sscanf (DEFAULT_CAL_PARAMS, "%d %d %d %d %d %d %d",
	      &defcal.a[1], &defcal.a[2], &defcal.a[0],
	      &defcal.a[4], &defcal.a[5], &defcal.a[3],
	      &defcal.a[6]);
  if (items != 7)
  {
    return 0;
  }
  return 1;
}


/*
 * set 'factory default' values
 */
static uint
reset_calibration (x_info *xinfo)
{
/**FIXME: write Xorg.conf? */
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
 * Use middle point to calculate min and max device coordinates of x and y
 * Middle point and target point device and screen coordinates are already
 * stored in cal
 */
static void 
extrapolate_dev_coords (int* xmin, int* xmax, int* ymin, int* ymax, \
						calibration* cal, x_info* xinfo)
{
  int X=xinfo->xres;
  int Y=xinfo->yres;
  float a1, a2;

  a1 = (float)(X - cal->xfb[1]) / X;
  a1 *= (float)(cal->x[4] * 2);
  a1 += (float) cal->x[1];
  a2 = (float)(X - cal->xfb[2]) / X;
  a2 *= (float)(cal->x[4] * 2);
  a2 += (float) cal->x[2];
  *xmax = (int)((a1+a2)/2);

  a1 = (float)(cal->xfb[0]) / X;
  a1 *= (float)(cal->x[4] * 2);
  a1 *= -1.0;
  a1 += (float) cal->x[0];
  a2 = (float)(cal->xfb[3]) / X;
  a2 *= (float)(cal->x[4] * 2);
  a2 *= -1.0;
  a2 += (float) cal->x[3];
  *xmin = (int)((a1+a2)/2);

  a1 = (float)(cal->yfb[0]) / Y;
  a1 *= (float)(cal->y[4] * 2);
  a1 += (float) cal->y[0];
  a2 = (float)(cal->yfb[1]) / Y;
  a2 *= (float)(cal->y[4] * 2);
  a2 += (float) cal->y[1];
  *ymin = (int)((a1+a2)/2);

  a1 = (float)(Y - cal->yfb[2]) / Y;
  a1 *= (float)(cal->y[4] * 2);
  a1 *= -1.0;
  a1 += (float) cal->y[2];
  a2 = (float)(Y - cal->yfb[3]) / Y;
  a2 *= (float)(cal->y[4] * 2);
  a2 *= -1.0;
  a2 += (float) cal->y[3];
  *ymax=(int)((a1+a2)/2);
  
}

/* Write Xorg.conf with new calibration parameters */
static int write_config (int xmin, int xmax, int ymin, int ymax) {
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  const char* expr1 = "Touchscreen";
  const char* expr2 = "Calibration";
  char section_ts = 0;
  char temp_filename[] = "/etc/xorg.confXXXXXX";
  int fd;
  int i;
  FILE* fp_conf = fopen (XCONF, "r");
  char* calibration_values = calloc (512,sizeof(char));

  /*FIXME rotation components are currently not used by xserver, set to 0 */
  i = snprintf (calibration_values, 512, "         Option       \
	 \"Calibration\"   \"%d %d %d %d %d %d\"\n", xmin, xmax, ymin, ymax ,0,0);
  
  if (i<=0)
	 goto error;

  if (!fp_conf){
	 ERROR ("could not open xorg.conf\n");
	 goto error;
  }
  
  fd = mkstemp(temp_filename);
  if (fd == -1) {
    ERROR ("Couldn't open temporary file\n");
	goto error;
  }

  i = 0;
  while ((read = getline(&line, &len, fp_conf)) != -1) {
	if (!section_ts && strstr(line,expr1)) {
	  section_ts=1;
      i = write (fd, line, read);
	} 
	else if (section_ts && strstr (line, expr2)){
   /* replace line with new calibration parameters */
	   i = write (fd, calibration_values, strlen(calibration_values));
	} else {
	   i = write (fd, line, read);
	}
	
	if (i==-1) {
		 ERROR ("could not write temp file\n");
		 goto error;
	  }
  }

  if (line)
     free(line);

  fclose (fp_conf);
  close (fd);
  sync();

  /* overwrite xorg.conf */
  if(rename(temp_filename, XCONF)) {
	 ERROR ("could not overwrite xorg.conf\n");
	 goto error;
  }

  sync ();
  return 1;

error:
  fclose (fp_conf);
  close (fd);
  return 0;
}

/*
 * sorting functions for qsort
 */

static int sort_by_x (const void* a, const void *b)
{
   return (((struct ts_sample *)a)->x - ((struct ts_sample *)b)->x);
}
static int sort_by_y (const void* a, const void *b)
{
   return (((struct ts_sample *)a)->y - ((struct ts_sample *)b)->y);
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

/**NOTE: modifications based on tslib code*/
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

  unsigned int i;

#ifdef ARM_TARGET
#define MAX_SAMPLES 256
  struct ts_sample samp[MAX_SAMPLES];
#endif

  draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_NEXT_TARGET);
  blit_active_hotspot (&xinfo, ACTIVE_HOTSPOT);

  /* swallow events */
  XFlush (xinfo.dpy);
  XSync (xinfo.dpy, 1);

  usleep (250000);

  
  XSynchronize (xinfo.dpy, True);
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

	if (ev.type == evtypes[TYPE_MOTION] || \
		ev.type == evtypes[TYPE_BPRESS] || \
		ev.type == evtypes[TYPE_BRELEASE])
    {
      int index   = 0;
      int middle  = 0;
      int raw_x   = 0;
      int raw_y   = 0;
      int trans_x = 0;
      int trans_y = 0;

/* for some reason XInput provides different raw data than ts_read */
	int ret;
	index = 0;
	do {
		if (index < MAX_SAMPLES-1)
			index++;
		if (ts_read_raw(ts, &samp[index], 1) < 0) {
			perror("ts_read");
			exit(1);
		}
	} while (samp[index].pressure > 0);
	
	XFlush(xinfo.dpy);
	XSync (xinfo.dpy,1);

	middle = index/2;

	qsort(samp, index, sizeof(struct ts_sample), sort_by_x);
	if (index & 1){
	  raw_x = samp[middle].x;
	}
    else {
      raw_x = (samp[middle-1].x + samp[middle].x) / 2;
    }

	qsort(samp, index, sizeof(struct ts_sample), sort_by_y);
    if (index & 1) {
      raw_y = samp[middle].y;
    }
    else {
      raw_y = (samp[middle-1].y + samp[middle].y) / 2;
    }

	  trans_x = raw_x;
	  trans_y = raw_y;

	/*translate device coords to screen coords for deviation check */
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


	   /* we are finished, calculate extrapolated min and max values
		* give parameters to xserver */
	   int minx, maxx, miny, maxy;
	   extrapolate_dev_coords(&minx, &maxx, &miny, &maxy, &cal, &xinfo);

	   /* FIXME: currently HAL support is not available in xserver, let's write
		* to Xorg.conf... */
		if (!write_config(minx, maxx, miny, maxy)) {
		   ERROR ("Could not write xorg.conf\n");
		   goto exit;
		}

		  ACTIVE_HOTSPOT = 42;

		  draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_COMPLETE);

		  XFlush (xinfo.dpy);
		  XSync (xinfo.dpy, 1);

		  usleep (2500000);

		  /* FIXME temporary solution until HAL can be used */
		  draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_RESTART);
		  XFlush (xinfo.dpy);
		  XSync (xinfo.dpy, 1);
		  usleep (2500000);

		  goto exit;
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
	} /* if it was tap or motion event */
	else if (ev.type == evtypes[TYPE_KRELEASE]) {
	XDeviceKeyEvent *key = (XDeviceKeyEvent *) &ev;
	
	  if (key->keycode == CANCEL)
	  {
	    /*
	     * user exit might be handled differently
	     */
	    goto exit;
	  }
	}
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

  char* tsdevice = strdup("/dev/input/ts");

#ifdef ARM_TARGET 
  ts = ts_open(tsdevice,0);

  if (!ts) {
    	ERROR("ts_open");
		exit(1);
  }
  if (ts_config(ts)) {
     	ERROR("ts_config");
		exit(1);
  }
#endif

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

/*NOTE using XSelectExtensionEvent */
/*
  if (XGrabKeyboard(xinfo.dpy,
                    xinfo.win,
                    False,
                    GrabModeAsync,
                    GrabModeAsync,
                    CurrentTime) == AlreadyGrabbed)
    goto away;
*/
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

  calibration_event_loop();

  free_graphics (&xinfo);

  away :
    return 0;
}

