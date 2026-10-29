#define XRT_MODULE_MATH
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/linalg_native.h"
#include <float.h>
#include <math.h>
#include <stdio.h>

static unsigned cases, failures;

static void check(bool ok, const char* label)
{
    if (!ok) { fprintf(stderr, "linalg numeric case %u: %s\n", cases, label); ++failures; }
    ++cases;
}

static bool near(double actual, double expected)
{
    return expected == 0.0 ? actual == 0.0 :
        isfinite(actual) && fabs(actual / expected - 1.0) <= 2.0 * DBL_EPSILON;
}

int main(void)
{
    static const double endpoints[][2] = {
        {1e308, 1.0}, {-1e308, -1.0}, {1e100, 1e-100},
        {-1e100, -1e-100}, {0.0, -0.0}, {-0.0, 0.0}
    };
    xmemdebugsnapshot before, after;
    xrtSetErrorInfo(XERR_VALUE, "linalg.test", 0, "warm diagnostic slot");
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (size_t i = 0; i < sizeof(endpoints) / sizeof(endpoints[0]); ++i) {
        double a = endpoints[i][0], b = endpoints[i][1];
        double first = xlin_lerp(a, b, 0.0), last = xlin_lerp(a, b, 1.0);
        check(first == a && !!signbit(first) == !!signbit(a) && !xrtGetError(), "lerp first endpoint");
        check(last == b && !!signbit(last) == !!signbit(b) && !xrtGetError(), "lerp last endpoint");
    }
    check(isnan(xlin_lerp(NAN, 1.0, 0.0)) && !xrtGetError(), "lerp NaN propagation");
    check(isnan(xlin_lerp(1.0, NAN, 1.0)) && !xrtGetError(), "lerp NaN last propagation");
    check(isnan(xlin_lerp(INFINITY, 1.0, 0.0)) && !xrtGetError(), "lerp nonfinite propagation");

    /* 原矩阵与真逆都用精确二次幂表示，覆盖行列置换和行列式符号。 */
    static const int scales[] = {512, 768, 990, 1023};
    static const int exponents[] = {1022, 1023, 1024, 1048, 1074};
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i)
      for (size_t j = 0; j < sizeof(exponents) / sizeof(exponents[0]); ++j)
        for (unsigned negative = 0; negative < 2; ++negative)
          for (unsigned permutation = 0; permutation < 4; ++permutation) {
            double large = ldexp(1.0, scales[i]);
            double small = ldexp(negative ? -1.0 : 1.0, scales[i] - exponents[j]);
            double input[] = {large, 0, large, small};
            double expected[] = {ldexp(1.0, -scales[i]), 0,
                -ldexp(negative ? -1.0 : 1.0, exponents[j] - scales[i]),
                ldexp(negative ? -1.0 : 1.0, exponents[j] - scales[i])};
            double matrix[4], inverse[4], result[] = {11, 22, 33, 44};
            unsigned swap_rows = permutation & 1u, swap_cols = permutation >> 1;
            for (unsigned row = 0; row < 2; ++row)
              for (unsigned col = 0; col < 2; ++col) {
                matrix[row * 2 + col] = input[(row ^ swap_rows) * 2 + (col ^ swap_cols)];
                inverse[row * 2 + col] = expected[(row ^ swap_cols) * 2 + (col ^ swap_rows)];
            }
            bool ok = xlin_inverse2_into(matrix[0], matrix[1], matrix[2], matrix[3],
                &result[0], &result[1], &result[2], &result[3]) != 0;
            for (unsigned k = 0; k < 4; ++k) ok = ok && near(result[k], inverse[k]);
            check(ok && !xrtGetError(), "scaled inverse finite result");
          }
    {
        double a, b, c, d;
        double scale = ldexp(1.0, -500);
        bool ok = xlin_inverse2_into(ldexp(1.0, -1074), 1.0, 0.75 * scale, scale,
            &a, &b, &c, &d) != 0;
        check(ok && near(a, -4.0 / 3.0) && near(b, ldexp(4.0 / 3.0, 500)) &&
            near(c, 1.0) && near(d, -ldexp(4.0 / 3.0, -574)) && !xrtGetError(),
            "subnormal intermediate must not lose precision");
    }
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount, "allocation counters changed");
    printf("linalg numeric edges C: %u cases, %u failures\n", cases, failures);
    return failures ? 1 : 0;
}

#include "../linalg_native.c"
