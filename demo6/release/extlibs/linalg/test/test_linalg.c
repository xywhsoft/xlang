/* 不依赖 XLang 调用器，直接验证 C 数值和 xvalue 桥。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/linalg_native.h"

#include <math.h>
#include <stdio.h>

xvalue* xlin_cross3(double ax, double ay, double az,
    double bx, double by, double bz);

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "linalg C test failed at line %d: %s\n", \
        __LINE__, #expr); return 2; \
} } while (0)

#define CHECK_SCALAR_RECOVERY(expr) do { \
    xrtSetErrorInfo(XERR_VALUE, "test.linalg", 0, "previous error"); \
    CHECK((expr) && xrtGetError() == NULL); \
} while (0)

int main(void)
{
    double x, y, a, b, c, d, z;
    xvalue* Cross;

    CHECK(xlin_dot2(1, 2, 3, 4) == 11);
    CHECK(xlin_dot3(1, 2, 3, 4, 5, 6) == 32);
    CHECK(xlin_mag2(3, 4) == 5);
    CHECK(xlin_mag3(2, 3, 6) == 7);
    CHECK(xlin_det2(1, 2, 3, 4) == -2);
    CHECK(xlin_dist2(0, 0, 3, 4) == 5);
    CHECK(xlin_dist3(0, 0, 0, 2, 3, 6) == 7);
    CHECK(xlin_normalize2_into(3, 4, &x, &y));
    CHECK(fabs(x - 0.6) < 1e-15 && fabs(y - 0.8) < 1e-15);
    CHECK(xlin_normalize2_into(1e-300, 0, &x, &y) && x == 1 && y == 0);
    CHECK(xlin_normalize2_into(1e300, 0, &x, &y) && x == 1 && y == 0);
    x = 123;
    y = 456;
    CHECK(!xlin_normalize2_into(0, 0, &x, &y) && x == 123 && y == 456);
    CHECK(!xlin_normalize2_into(INFINITY, 1, &x, &y));
    CHECK(!xlin_normalize2_into(NAN, 1, &x, &y));
    CHECK(!xlin_normalize2_into(1, 2, NULL, &y));
    CHECK(xrtGetError() != NULL);
    x = 123;
    CHECK(!xlin_normalize2_into(3, 4, &x, &x) && x == 123 &&
        xrtGetError() != NULL);
    CHECK(xlin_dot2(1, 2, 3, 4) == 11 && xrtGetError() == NULL);
    CHECK_SCALAR_RECOVERY(xlin_dot3(1, 2, 3, 4, 5, 6) == 32);
    CHECK_SCALAR_RECOVERY(xlin_mag2(3, 4) == 5);
    CHECK_SCALAR_RECOVERY(xlin_mag3(2, 3, 6) == 7);
    CHECK_SCALAR_RECOVERY(xlin_det2(1, 2, 3, 4) == -2);
    CHECK_SCALAR_RECOVERY(xlin_dist2(0, 0, 3, 4) == 5);
    CHECK_SCALAR_RECOVERY(xlin_dist3(0, 0, 0, 2, 3, 6) == 7);
    CHECK_SCALAR_RECOVERY(xlin_lerp(1, 3, 0.5) == 2);

    CHECK(xlin_inverse2_into(1, 2, 3, 4, &a, &b, &c, &d));
    CHECK(a == -2 && b == 1 && c == 1.5 && d == -0.5);
    CHECK(xlin_inverse2_into(1e-8, 0, 0, 1e-8, &a, &b, &c, &d));
    CHECK(fabs(a / 1e8 - 1) < 1e-15 && d == a && b == 0 && c == 0);
    CHECK(xlin_inverse2_into(1e300, 0, 0, 1e-300, &a, &b, &c, &d));
    CHECK(fabs(a / 1e-300 - 1) < 1e-15 &&
        fabs(d / 1e300 - 1) < 1e-15 && b == 0 && c == 0);
    a = 123;
    b = 456;
    c = 789;
    d = 999;
    CHECK(!xlin_inverse2_into(1, 2, 2, 4, &a, &b, &c, &d) &&
        a == 123 && b == 456 && c == 789 && d == 999);
    CHECK(!xlin_inverse2_into(1, 1, 1, 1 + 2.220446049250313e-16,
        &a, &b, &c, &d));
    CHECK(!xlin_inverse2_into(INFINITY, 0, 0, 1, &a, &b, &c, &d));
    a = 123;
    b = 456;
    c = 789;
    CHECK(!xlin_inverse2_into(1, 2, 3, 4, &a, &b, &c, &a) &&
        a == 123 && b == 456 && c == 789 && xrtGetError() != NULL);

    /* 不对称矩阵和不同量级的行缩放，核对 A * inverse(A) = I。 */
    {
        unsigned int seed = 0x51a7u;
        int sample, checked = 0;
        for (sample = 0; sample < 1024; ++sample) {
            double ma, mb, mc, md, row1, row2;
            double det;
            seed = seed * 1664525u + 1013904223u;
            ma = (double)((seed >> 16) % 21u) - 10.0;
            seed = seed * 1664525u + 1013904223u;
            mb = (double)((seed >> 16) % 21u) - 10.0;
            seed = seed * 1664525u + 1013904223u;
            mc = (double)((seed >> 16) % 21u) - 10.0;
            seed = seed * 1664525u + 1013904223u;
            md = (double)((seed >> 16) % 21u) - 10.0;
            det = ma * md - mb * mc;
            if (fabs(det) < 4.0) continue;
            ++checked;
            row1 = sample % 3 == 0 ? 1e-2 : sample % 3 == 1 ? 1.0 : 1e2;
            row2 = sample % 3 == 0 ? 1e2 : sample % 3 == 1 ? 1e-2 : 1.0;
            ma *= row1;
            mb *= row1;
            mc *= row2;
            md *= row2;
            CHECK(xlin_inverse2_into(ma, mb, mc, md, &a, &b, &c, &d));
            CHECK(fabs(ma * a + mb * c - 1.0) <
                1e-12 * (fabs(ma * a) + fabs(mb * c) + 1.0));
            CHECK(fabs(ma * b + mb * d) <
                1e-12 * (fabs(ma * b) + fabs(mb * d) + 1.0));
            CHECK(fabs(mc * a + md * c) <
                1e-12 * (fabs(mc * a) + fabs(md * c) + 1.0));
            CHECK(fabs(mc * b + md * d - 1.0) <
                1e-12 * (fabs(mc * b) + fabs(md * d) + 1.0));
        }
        CHECK(checked >= 900);
    }

    CHECK(fabs(xlin_angle2(1e300, 0, 0, 1e300) - acos(0)) < 1e-15);
    CHECK(fabs(xlin_angle2(1e-300, 0, 0, 1e-300) - acos(0)) < 1e-15);
    CHECK(fabs(xlin_angle2(1, 0, 1, 1e-8) - 1e-8) < 1e-12);
    CHECK(fabs((acos(-1) - xlin_angle2(1, 0, -1, 1e-8)) - 1e-8) < 1e-12);
    CHECK(isnan(xlin_angle2(0, 0, 1, 0)));
    CHECK(isnan(xlin_angle2(NAN, 1, 1, 0)));
    CHECK(xlin_lerp(-1e308, 1e308, 0.5) == 0);
    CHECK(xlin_lerp(-1e308, 1e308, -1e-100) == -1e308);
    CHECK(isfinite(xlin_lerp(-1e308, 1e308, 1.000000000000001)) &&
        xlin_lerp(-1e308, 1e308, 1.000000000000001) > 1e308);

    CHECK(xlin_cross3_into(1, 0, 0, 0, 1, 0, &x, &y, &z));
    CHECK(x == 0 && y == 0 && z == 1);
    x = 123;
    y = 456;
    CHECK(!xlin_cross3_into(1, 0, 0, 0, 1, 0, &x, &y, &x) &&
        x == 123 && y == 456 && xrtGetError() != NULL);
    CHECK(xlin_multiply2_into(1, 2, 3, 4, -2, 1, 1.5, -0.5,
        &a, &b, &c, &d));
    CHECK(a == 1 && b == 0 && c == 0 && d == 1);
    a = 123;
    b = 456;
    d = 999;
    CHECK(!xlin_multiply2_into(1, 2, 3, 4, -2, 1, 1.5, -0.5,
        &a, &b, &b, &d) && a == 123 && b == 456 && d == 999 &&
        xrtGetError() != NULL);
    Cross = xlin_cross3(1, 0, 0, 0, 1, 0);
    CHECK(Cross != NULL && xrtValueCount(Cross) == 3u &&
        xrtValueGetFloat(xrtValueArrayGet(Cross, 2u), &z) && z == 1);
    xrtValueRelease(Cross);
    {
        size_t fail_at;
        for (fail_at = 0; fail_at < 6; ++fail_at) {
            int triggered;
            CHECK(xrtMemDebugFailAfter(fail_at));
            Cross = xlin_cross3(1, 0, 0, 0, 1, 0);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(Cross && xrtValueType(Cross) == XVALUE_NULL &&
                    xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            } else {
                CHECK(Cross && xrtValueType(Cross) == XVALUE_ARRAY &&
                    xrtGetError() == NULL);
            }
            xrtValueRelease(Cross);
            xrtClearError();
        }
    }
    Cross = xlin_cross3(1, 0, 0, 0, 1, 0);
    CHECK(Cross && xrtValueCount(Cross) == 3u);
    xrtValueRelease(Cross);
    puts("linalg C boundaries: PASS");
    return 0;
}
