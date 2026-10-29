#define XRT_MODULE_MATH
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../include/linalg_native.h"
#include <math.h>
#include <stdio.h>

int main(void)
{
    double a, b, c, d;
    if (xlin_lerp(1e16, 1.0, 1.0) != 1.0) return 1;
    puts("lerp endpoint: true");
    double large = ldexp(1.0, 1023), small = ldexp(1.0, -51);
    if (!xlin_inverse2_into(large, 0.0, large, small, &a, &b, &c, &d) ||
        a != ldexp(1.0, -1023) || b != 0.0 || c != -ldexp(1.0, 51) || d != -c) return 1;
    puts("large inverse: true");
    /* 相同校准矩阵、输入坐标与容差，C直接写输出参数，不做xvalue装箱。 */
    double channel_x = (5.0 + 2.0 * 7.0) * 1e100;
    double channel_y = (3.0 * 5.0 + 4.0 * 7.0) * 1e-100;
    if (!xlin_inverse2_into(1e100, 2e100, 3e-100, 4e-100, &a, &b, &c, &d) ||
        fabs(a * channel_x + b * channel_y - 5.0) >= 1e-12 ||
        fabs(c * channel_x + d * channel_y - 7.0) >= 1e-12) return 1;
    puts("restored point: true");
    return 0;
}

#ifdef __TINYC__
#include "../linalg_native.c"
#endif
