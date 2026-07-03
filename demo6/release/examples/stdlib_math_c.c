#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>

static int near_value(double actual, double expected)
{
    return fabs(actual - expected) <= 1e-9;
}

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

int main(void)
{
    double radius = 3.0;
    double area = pow(radius, 2.0) * XRT_MATH_PI;
    double diagonal = sqrt(2.0);

    require_line("basic",
        near_value(area, 28.274333882308138) &&
        near_value(diagonal, 1.414213562373095) &&
        near_value(fabs(-3.5), 3.5));

    require_line("trig",
        near_value(sin(0.0), 0.0) &&
        near_value(cos(0.0), 1.0) &&
        near_value(tan(0.0), 0.0) &&
        near_value(asin(0.0), 0.0) &&
        near_value(acos(1.0), 0.0) &&
        near_value(atan(0.0), 0.0) &&
        near_value(atan2(0.0, 1.0), 0.0));

    require_line("exp_log",
        near_value(exp(0.0), 1.0) &&
        near_value(log(1.0), 0.0) &&
        near_value(log10(100.0), 2.0) &&
        near_value(xrtMathLog2(8.0), 3.0) &&
        near_value(xrtMathExp2(4.0), 16.0) &&
        near_value(xrtMathLog1p(1.0), log(2.0)) &&
        near_value(xrtMathExpm1(1.0), XRT_MATH_E - 1.0));

    require_line("rounding",
        near_value(floor(1.9), 1.0) &&
        near_value(ceil(1.1), 2.0) &&
        near_value(floor(1.6 + 0.5), 2.0) &&
        near_value(xrtMathTrunc(-1.9), -1.0) &&
        near_value(xrtMathFract(-1.25), 0.75) &&
        near_value(xrtMathMod(5.5, 2.0), 1.5));

    require_line("constants",
        near_value(XRT_MATH_PI, 3.141592653589793) &&
        near_value(XRT_MATH_TAU, XRT_MATH_PI * 2.0) &&
        near_value(XRT_MATH_E, 2.718281828459045) &&
        xrtMathIsNaN(XRT_MATH_NAN) &&
        xrtMathIsInf(XRT_MATH_INF) &&
        xrtMathIsFinite(XRT_MATH_PI) &&
        !xrtMathIsFinite(XRT_MATH_INF));

    require_line("range",
        near_value(xrtMathMin(-2.0, 5.0), -2.0) &&
        near_value(xrtMathMax(-2.0, 5.0), 5.0) &&
        near_value(xrtMathClamp(12.0, 0.0, 10.0), 10.0) &&
        near_value(xrtMathClamp(-2.0, 0.0, 10.0), 0.0) &&
        xrtMathSign(-3.0) == -1 &&
        xrtMathSign(0.0) == 0 &&
        xrtMathSign(3.0) == 1);

    require_line("advanced",
        near_value(xrtMathRad(180.0), XRT_MATH_PI) &&
        near_value(xrtMathDeg(XRT_MATH_PI), 180.0) &&
        near_value(xrtMathCbrt(27.0), 3.0) &&
        near_value(xrtMathCbrt(-8.0), -2.0) &&
        near_value(xrtMathHypot(3.0, 4.0), 5.0));

    return 0;
}
