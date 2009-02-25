
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libosso.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <hildon-cp-plugin/hildon-cp-plugin-interface.h>

#include "config.h"

/* sudo needed on device for accessing files in /etc and using tslib */
#ifdef ARM_TARGET
#define LAUNCH_PAR_AMOUNT 5
#else
#define LAUNCH_PAR_AMOUNT 4
#endif

#define WINDOW_PAR_MAXLENGTH 16

#define BINARY "/usr/bin/tscalibrate"

osso_return_t execute(osso_context_t *osso, gpointer data, gboolean user_activated);
int reset(void);

/*
 * Watch function, will quit gtk_main when child returns
 *
 */
static void
child_watch (GPid pid,
	     gint status,
	     gpointer data)
{
  g_spawn_close_pid(pid);
  gtk_main_quit();
}


/*
 * called from execute() and reset()
 */
static int
my_execute(char *str, int xid)
{
  int i;
  int status = OSSO_OK;
  gchar **cmd = calloc (LAUNCH_PAR_AMOUNT, sizeof(gchar*));

  GPid child_pid;
  GError *error = NULL;
  
  /* build parameters, <sudo> binary <option> <id> */
  i=0;
#ifdef ARM_TARGET
  cmd[i++] = strdup("/usr/bin/sudo");
#endif
  cmd[i++] = strdup(BINARY);
  cmd[i++] = strdup(str);
  cmd[i++] = (char*)calloc(1, WINDOW_PAR_MAXLENGTH);
  cmd[i++] = (char*)calloc(1, 1);

  sprintf(cmd[LAUNCH_PAR_AMOUNT-2], "%d", xid);
  cmd[LAUNCH_PAR_AMOUNT-1] = '\0';

  /* handle all events in queue */
  while (gtk_events_pending())
  {
    gtk_main_iteration();
  }

  /* spawn child process */
  if(!g_spawn_async(NULL,
		cmd,
		NULL,
		G_SPAWN_LEAVE_DESCRIPTORS_OPEN|G_SPAWN_DO_NOT_REAP_CHILD,
		NULL,
		NULL,
		&child_pid,
		&error))
  {
    status = OSSO_ERROR;
  }

  /* watch for child to exit */
  g_child_watch_add (child_pid,
		     child_watch,
		     NULL);
 
  /* run gtk main loop */
  gtk_main();
  
  /* free parameters */
  for (i=0; i<LAUNCH_PAR_AMOUNT; i++)
    free(cmd[i]);
  free(cmd);

  return status;
}

/*
 * executes tscalibrate program with parent window as a parameter
 */
osso_return_t
execute(osso_context_t *osso, gpointer data, gboolean user_activated)
{
  int xid = 0;
  if (data != NULL)
  {
    xid = GDK_WINDOW_XID(((GtkWidget*)data)->window);
  }

  /* execute with parent window id as parameter */
  return (my_execute("cpanel", xid));
}

/*
 * executes tscalibrate with 'reset' as a parameter
 */
int
reset(void)
{
  return (my_execute("reset", 0));
}

