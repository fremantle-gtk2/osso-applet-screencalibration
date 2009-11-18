
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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <linux/input.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput.h>

#include "common.h"
#include "gfx.h"

#define HOTSPOT_AMOUNT 3

#define ANIM_TIMEOUT 30000

typedef struct {
        int x[4], xfb[4];
        int y[4], yfb[4];
        int a[7];
} calibration;

static x_info xinfo;
static calibration cal;

#ifdef ARM_TARGET
#define DEFAULT_CAL_PARAMS "14114 18 -2825064 34 -8765 32972906 65536"
#define DEFAULT_EVDEV_PARAMS "172 3880 3780 235"
#define XCONF "/usr/share/hal/fdi/policy/10osvendor/10-x11-input.fdi"
#define MAX_SAMPLES 256
static int ts, kb;
static calibration defcal;
#endif /* ifdef ARM_TARGET */

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
/* Write x11-input.fdi with new calibration parameters */
static int write_config (cal_evdev *p) {
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  const char* expr1 = "input.touchpad";
  const char* expr2 = "Calibration";
  char section_ts = 0;
  char temp_filename[] = "/usr/share/x11-input.fdi.XXXXXX";
  int fd = -1;
  int i;
  FILE* fp_conf = fopen (XCONF, "r");
  char* calibration_values = calloc (512,sizeof(char));

  /*FIXME rotation components are currently not used by xserver, set to 0 */
  i = snprintf (calibration_values, 512, "\t\t\t<merge key=\"input.x11_options.Calibration\" type=\"string\">%d %d %d %d %d %d</merge>\n", p->params[0], p->params[1],
	 p->params[2], p->params[3], 0,0);
  
  if (i<=0)
	 goto error;

  if (!fp_conf){
	 ERROR ("could not open %s\n", XCONF);
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
  if (fd != -1) { close (fd); }
  return 0;
}

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
  cal_evdev reset;
  uint items;

  items = sscanf (DEFAULT_EVDEV_PARAMS, "%d %d %d %d",
	      &reset.params[0], &reset.params[1], &reset.params[2], &reset.params[3]);
  
  if (items != 4)
  {
    return 0;
  }

  if (!write_config(&reset)) {
    ERROR ("Could not write xorg.conf\n");
    return 0;
  }

  set_calibration_prop (xinfo, &reset);
  XFlush (xinfo->dpy);
  XSync (xinfo->dpy, 1);

  usleep (250000);

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
 * Use target point device coordinates, which are already
 * stored in cal
 */
static void 
extrapolate_dev_coords (cal_evdev *p, calibration* cal, x_info* xinfo)
{
  int X=xinfo->xres;
  int Y=xinfo->yres;
  float min, max, delta;

  /* x */
  min = (float)((cal->x[0]+cal->x[3])/2);
  max = (float)((cal->x[1]+cal->x[2])/2);
  delta = (max - min) / ((float)(X - 2*POINT_SCR_DST_X)/POINT_SCR_DST_X);
  min -= delta;
  max += delta;

  p->params[0] = (int) min;
  p->params[1] = (int) max;

  /* y */
  min = (float)((cal->y[0]+cal->y[1])/2);
  max = (float)((cal->y[2]+cal->y[3])/2);
  delta = (min - max) / ((float)(Y - 2*POINT_SCR_DST_Y)/POINT_SCR_DST_Y);
  min += delta;
  max -= delta;

  p->params[2] = (int) min;
  p->params[3] = (int) max;

  /* debugging output */
  int i;
  ERROR ("extrapolated parameters:\n");
  for (i = 0; i<4; i++) {
     ERROR ("%d\n",p->params[i]);
  }
  ERROR ("------------------\n");
}

/*
 * sorting functions for qsort
 */

static int sort_by_x (const void* a, const void *b)
{
   return (((ts_sample *)a)->x - ((ts_sample *)b)->x);
}
static int sort_by_y (const void* a, const void *b)
{
   return (((ts_sample *)a)->y - ((ts_sample *)b)->y);
}
/*****************************************************************/
#endif /* ifdef ARM_TARGET */

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
		       (unsigned int*) num_children);

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

#ifdef ARM_TARGET
static int read_ts_events (int dev, ts_sample* samp) {
  struct input_event ev[512];
  int read_bytes;
  int index = 0;
  int yalv;
  errno = 0;

  read_bytes = read(dev, ev, sizeof(struct input_event) * 512);

  if (read_bytes == -1) {
	if (errno == EAGAIN) {
	  return 0;
    } else { return -1;	}
  }
    
  if (read_bytes < (int) sizeof(struct input_event)) {
    perror("evdev: short read");
    exit (1);
  }

  for (yalv = 0; yalv < (int) (read_bytes / sizeof(struct input_event)) 
	   && index < MAX_SAMPLES; yalv++)
  {
       if (ev[yalv].type == EV_ABS ) {
	     switch (ev[yalv].code) {
		   case ABS_X:
     		 samp[index].x = ev[yalv].value; 
			 break;
		   case ABS_Y:
   			 samp[index].y = ev[yalv].value;
			 break;
	     }
		 if (samp[index].x && samp[index].y)
			index++;
	   }
  }
  return index;
}

static int read_key_events (int dev, int* keys) {
  struct input_event ev[512];
  int read_bytes;
  int index = 0;
  int yalv;
  errno = 0;

  read_bytes = read(dev, ev, sizeof(struct input_event) * 512);
  
  if (read_bytes == -1) {
	if (errno == EAGAIN) {
	  return 0;
    } else { return -1;	}
  }
  if (read_bytes < (int) sizeof(struct input_event)) {
    perror("evdev: short read");
    exit (1);
  }

  for (yalv = 0; yalv < (int) (read_bytes / sizeof(struct input_event)) 
	   && index < MAX_SAMPLES; yalv++) {
    if (ev[yalv].type == EV_KEY) {
    /* add keycode when released */
	  if (ev[yalv].value == 0)
		 keys[index++] = ev[yalv].code;
	}
  }
  return index;
}
#endif /* #ifdef ARM_TARGET */

/*
 * read events & draw graphics
 */
static void
calibration_event_loop (void)
{
  XEvent ev;
  int event_amount;
  int ACTIVE_HOTSPOT = 0;

  unsigned int i;

#ifdef ARM_TARGET
  ts_sample *samp = NULL;
#endif

  draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_NEXT_TARGET);
  XFlush (xinfo.dpy); 
  XSync (xinfo.dpy, 1); 

  blit_active_hotspot (&xinfo, ACTIVE_HOTSPOT);

  /* swallow events */
  XFlush (xinfo.dpy);
  XSync (xinfo.dpy, 1);
  usleep (250000);

  for (;;)
    {
      usleep (20000);

#ifdef ARM_TARGET
      int index   = 0;
      int middle  = 0;
      int raw_x   = 0;
      int raw_y   = 0;
      int trans_x = 0;
      int trans_y = 0;
 
	  if (samp) free(samp);
	  samp = calloc (MAX_SAMPLES, sizeof (ts_sample));

      index = read_ts_events (ts, samp);

	  if (index == -1) {
		 perror ("error reading samples from touchscreen");
		 goto exit;
	  } else if (index)
	  {
	    middle = index/2;
		qsort(samp, index, sizeof(ts_sample), sort_by_x);
		if (index & 1){
		  raw_x = samp[middle].x;
		} else {
		  raw_x = (samp[middle-1].x + samp[middle].x) / 2;
		}

		qsort(samp, index, sizeof(ts_sample), sort_by_y);
		if (index & 1) {
		  raw_y = samp[middle].y;
		} else {
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
              /* calculate extrapolated min and max values
	        * give parameters to xserver */
		    cal_evdev result;
		    extrapolate_dev_coords(&result, &cal, &xinfo);

	   if (!write_config(&result)) {
		   ERROR ("Could not write xinput fdi file\n");
		   goto exit;
		}

		    ACTIVE_HOTSPOT = 42;
		    draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_COMPLETE);

		    /* set calibration device property */
		    set_calibration_prop (&xinfo, &result);
		    XFlush (xinfo.dpy);
		    XSync (xinfo.dpy, 1);
		    usleep (2500000);

		    goto exit;
		  }

	      /* neeext! */
	      draw_screen (&xinfo, ACTIVE_HOTSPOT, TAP_NEXT_TARGET);
	      XFlush (xinfo.dpy);
	      XSync (xinfo.dpy, 1);
	      usleep (500000);

	      /* flush buffer */
	      if (samp) free(samp);
	      samp = calloc (MAX_SAMPLES, sizeof (ts_sample));
	      read_ts_events (ts, samp);
	      usleep (500000);
	    }
	  else
	    {
	      draw_instructions (&xinfo, ACTIVE_HOTSPOT, TAP_CLOSER);
	      XFlush (xinfo.dpy);
	      XSync (xinfo.dpy, 1);
	    }
	  } /* if there was any event on ts */

	  /* check for keyboard input */
	  int kev[MAX_SAMPLES];
	  index = read_key_events (kb, kev);
	
	  if (index == -1) {
		 perror ("error reading samples from keypad");
		 goto exit;
	  }
	  /* do something with the input */
	  for (i=0; i<index; i++) {
		 if (kev[i] == KEY_BACKSPACE)
		   /* user exit */
		   goto exit;
	  }
#endif /* ifdef ARM_TARGET */

      /*
       * other events ...
       */
      event_amount = XEventsQueued (xinfo.dpy, QueuedAfterReading);

      if (event_amount < 1)
      {
	    blit_active_hotspot (&xinfo, ACTIVE_HOTSPOT);
	    XFlush(xinfo.dpy);
	    usleep (ANIM_TIMEOUT);
	    continue;
      }

	  XNextEvent (xinfo.dpy, &ev);
	  fprintf (stderr, "X event type: %d\n", ev.type);

    switch (ev.type)
    {

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
#endif /* ifndef ARM_TARGET */

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

    case LeaveNotify:{
      /* Exit when another window gets on top of us.*/
      int mtype = ((XLeaveWindowEvent*)&ev)->type;
      if (mtype == LeaveNotify)
        goto exit;
      }
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
  
#ifdef ARM_TARGET 
  if ((ts = open("/dev/input/ts", O_RDONLY | O_NONBLOCK)) < 0) {
	perror("evdev open");
	exit(1);
  }

  if ((kb = open("/dev/input/keypad", O_RDONLY | O_NONBLOCK)) < 0) {
	perror("evdev open");
	exit(1);
  }
#endif /* ifdef ARM_TARGET */

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

#else

  XGrabPointer (xinfo.dpy,
		xinfo.win,
		False,
		ButtonReleaseMask,
		GrabModeAsync,
		GrabModeAsync,
		xinfo.win,
		None,
		CurrentTime);
#endif /* ifdef ARM_TARGET */

   int grab = XGrabKeyboard(xinfo.dpy,
                    xinfo.win,
                    False,
                    GrabModeAsync,
                    GrabModeAsync,
                    CurrentTime);
   if (grab == AlreadyGrabbed) {
/*	g_debug ("AlreadyGrabbed!");*/
	}

  /* copy hotspot locations to calibration struct */

  cal.xfb[0] = xinfo.hotspots[0].x;  cal.yfb[0] = xinfo.hotspots[0].y;
  cal.xfb[1] = xinfo.hotspots[1].x;  cal.yfb[1] = xinfo.hotspots[1].y;
  cal.xfb[2] = xinfo.hotspots[2].x;  cal.yfb[2] = xinfo.hotspots[2].y;
  cal.xfb[3] = xinfo.hotspots[3].x;  cal.yfb[3] = xinfo.hotspots[3].y;

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

