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

#include <stdio.h>
#include <locale.h>
#include <libintl.h>

enum
  {
    TAP_CLOSER = 0,
    TAP_NEXT_TARGET,
    TAP_COMPLETE
  };

#define uint unsigned int

#define SUPPRESS_FORMAT_WARNING(x) ((char *)(long)(x))
#define _(string) SUPPRESS_FORMAT_WARNING(gettext (string))

#define ERROR(txt, args... ) fprintf(stderr, "osso-applet-screencalibration : " txt, ##args)

#endif
