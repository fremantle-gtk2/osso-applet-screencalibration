/*
 *  Copyright (C) 2001 Russell King.
 *
 * This file is placed under the GPL.  Please see the file
 * COPYING for more details.
 *
 */
typedef struct {
        int x[5], xfb[5];
        int y[5], yfb[5];
        int a[7];
} calibration;

/* function definitions */
int perform_calibration(calibration *cal);
