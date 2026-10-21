#define XRT_MODULE_ALL
#include "xrt.h"
#include "../../extlibs/linalg/include/linalg_native.h"

#include <math.h>
#include <stdio.h>

int main(void)
{
    double x, y, a, b, c, d, cx, cy, cz, ma, mb, mc, md, angle;
    int Ok = xlin_normalize2_into(3, 4, &x, &y) &&
        xlin_cross3_into(1, 0, 0, 0, 1, 0, &cx, &cy, &cz) &&
        xlin_inverse2_into(1, 2, 3, 4, &a, &b, &c, &d);
    if (Ok) {
        angle = xlin_angle2(1, 0, 0, 1);
        Ok = xlin_multiply2_into(1, 2, 3, 4, a, b, c, d,
            &ma, &mb, &mc, &md) && angle > 1.57 && angle < 1.58;
    }
    if (Ok) {
        printf("dot2:  %.1f\n", xlin_dot2(1, 2, 3, 4));
        printf("dot3:  %.1f\n", xlin_dot3(1, 2, 3, 4, 5, 6));
        printf("mag2:  %.1f\n", xlin_mag2(3, 4));
        printf("mag3:  %.1f\n", xlin_mag3(2, 3, 6));
        printf("normalize2(3,4):  %.1f ,  %.1f\n", x, y);
        printf("cross3:  %.1f ,  %.1f ,  %.1f\n", cx, cy, cz);
        printf("det2:  %.1f\n", xlin_det2(1, 2, 3, 4));
        printf("inverse2:  %.1f ,  %.1f ,  %.1f ,  %.1f\n", a, b, c, d);
        printf("matrix product:  %.1f ,  %.1f ,  %.1f ,  %.1f\n", ma, mb, mc, md);
        printf("dist2(0,0,3,4):  %.1f\n", xlin_dist2(0, 0, 3, 4));
        printf("dist3:  %.1f\n", xlin_dist3(0, 0, 0, 2, 3, 6));
        printf("lerp(2,8,0.25):  %.1f\n", xlin_lerp(2, 8, 0.25));
        puts("linalg C/XL example: PASS");
    }
    return Ok ? 0 : 2;
}

/* xl run 单文件执行时，TCC 不会读取扩展库构建清单。 */
#ifdef __TINYC__
#include "../../extlibs/linalg/linalg_native.c"
#endif
