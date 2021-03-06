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

#ifndef _OSSO_APPLET_SCREENCALIBRATION_COMMON_H_
#define _OSSO_APPLET_SCREENCALIBRATION_COMMON_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <locale.h>
#include <libintl.h>
#include <sys/unistd.h>

enum {
   TYPE_MOTION = 0,
   TYPE_BPRESS,
   TYPE_BRELEASE,
   TYPE_KRELEASE,
   NUM_TYPES /* always the last one */
};

int evtypes [NUM_TYPES];

enum
  {
    TAP_CLOSER = 0,
    TAP_NEXT_TARGET,
    TAP_COMPLETE
  };

/* for storing calculated min/max x and y for evdev driver */
typedef struct {
   int params[4];
} cal_evdev;

/* struct for storing raw information read from touchscreen */
typedef struct {
   int x;
   int y;
/* pressure is currently not used */
} ts_sample;

#define uint unsigned int

#define SUPPRESS_FORMAT_WARNING(x) ((char *)(long)(x))
#define _(string) SUPPRESS_FORMAT_WARNING(gettext (string))

#define ERROR(txt, args... ) fprintf(stderr, "osso-applet-screencalibration : " txt, ##args)

#endif
