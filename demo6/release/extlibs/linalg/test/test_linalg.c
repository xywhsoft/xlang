/* linalg C API smoke: direct output parameters and xvalue bridge agree. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <math.h>
#include <stdio.h>

int xlin_normalize2_into(double x, double y, double* px, double* py);
xvalue* xlin_cross3(double ax, double ay, double az, double bx, double by, double bz);

int main(void)
{
    double x;
    double y;
    xvalue* pCross;
    double z;

    if (!xlin_normalize2_into(3.0, 4.0, &x, &y) ||
        fabs(x - 0.6) > 1e-12 || fabs(y - 0.8) > 1e-12) {
        fputs("linalg C smoke failed: normalize2\n", stderr);
        return 1;
    }
    pCross = xlin_cross3(1.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    if (pCross == NULL || xrtValueCount(pCross) != 3u ||
        !xrtValueGetFloat(xrtValueArrayGet(pCross, 2u), &z) || z != 1.0) {
        xrtValueRelease(pCross);
        fputs("linalg C smoke failed: xvalue result\n", stderr);
        return 1;
    }
    xrtValueRelease(pCross);
    puts("linalg C smoke: PASS");
    return 0;
}

#include "../linalg_native.c"
