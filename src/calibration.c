/*
 *  tslib/tests/ts_calibrate.c
 *
 *  Copyright (C) 2001 Russell King.
 *
 * This file is placed under the GPL.  Please see the file
 * COPYING for more details.
 *
 * Basic test program for touchscreen library.
 */

#include "calibration.h"

/**
	Calculates the calibration values.
	This function was coded by Russell King.

	Calibration struct contains the user
	input data. Function makes a matrix and
	calculates the determinant. If determinant
	is too small (-0.1 .. 0.1) then calibration
	failed. Otherwise continue by calculating
	the result values. Values are stored in
	calibration struct and later saved in the
	main program.

	@param cal calibration struct	
	@return success boolean value

*/
int perform_calibration(calibration *cal) {
        int j;
        float n, x, y, x2, y2, xy, z, zx, zy;
        float det, a, b, c, e, f, i;
        float scaling = 65536.0;

// Get sums for matrix
        n = x = y = x2 = y2 = xy = 0;
        for(j=0;j<5;j++) {
                n += 1.0;
                x += (float)cal->x[j];
                y += (float)cal->y[j];
                x2 += (float)(cal->x[j]*cal->x[j]);
                y2 += (float)(cal->y[j]*cal->y[j]);
                xy += (float)(cal->x[j]*cal->y[j]);
        }

// Get determinant of matrix -- check if determinant is too small
        det = n*(x2*y2 - xy*xy) + x*(xy*y - x*y2) + y*(x*xy - y*x2);
        if(det < 0.1 && det > -0.1) {
                return 0;
        }

// Get elements of inverse matrix
        a = (x2*y2 - xy*xy)/det;
        b = (xy*y - x*y2)/det;
        c = (x*xy - y*x2)/det;
        e = (n*y2 - y*y)/det;
        f = (x*y - n*xy)/det;
        i = (n*x2 - x*x)/det;

// Get sums for x calibration
        z = zx = zy = 0;
        for(j=0;j<5;j++) {
                z += (float)cal->xfb[j];
                zx += (float)(cal->xfb[j]*cal->x[j]);
                zy += (float)(cal->xfb[j]*cal->y[j]);
        }

// Now multiply out to get the calibration for framebuffer x coord
        cal->a[0] = (int)((a*z + b*zx + c*zy)*(scaling));
        cal->a[1] = (int)((b*z + e*zx + f*zy)*(scaling));
        cal->a[2] = (int)((c*z + f*zx + i*zy)*(scaling));

// Get sums for y calibration
        z = zx = zy = 0;
        for(j=0;j<5;j++) {
                z += (float)cal->yfb[j];
                zx += (float)(cal->yfb[j]*cal->x[j]);
                zy += (float)(cal->yfb[j]*cal->y[j]);
        }

// Now multiply out to get the calibration for framebuffer y coord
        cal->a[3] = (int)((a*z + b*zx + c*zy)*(scaling));
        cal->a[4] = (int)((b*z + e*zx + f*zy)*(scaling));
        cal->a[5] = (int)((c*z + f*zx + i*zy)*(scaling));

// If we got here, we're OK, so assign scaling to a[6] and return
        cal->a[6] = (int)scaling;
        return 1;
}
