#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../statistics_native.h"
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../statistics_native.c"
#endif

static unsigned checks, failures;

static xvalue *numbers(const double *values, size_t count) {
    xvalue *result = xrtValueArray();
    if (!result) return NULL;
    for (size_t i = 0; i < count; ++i) {
        if (!xrtValueArrayAppendNew(result, xrtValueFloat(values[i]))) {
            xrtValueRelease(result); return NULL;
        }
    }
    return result;
}

static void compare(const char *name, xvalue *result, double expected) {
    double actual = NAN;
    ++checks;
    if (!result || xrtGetError() || !xrtValueGetFloat(result, &actual) ||
        !isfinite(actual) || fabs(actual-expected) > fabs(expected)*2e-13) {
        if (failures < 12) fprintf(stderr, "%s: %.17g != %.17g\n", name, actual, expected);
        ++failures;
    }
    xrtValueRelease(result);
}

/* 精确偏差为[0,h,h]，不把已经舍入的大均值拿来生成测试预期。 */
static int fixed_cases(void) {
    static const int exponents[] = {-440,-100,0,26,52,100,400,540};
    const double dependent[] = {0,1,1};
    for (size_t e = 0; e < sizeof(exponents)/sizeof(exponents[0]); ++e) {
        for (int sign = -1; sign <= 1; sign += 2) {
            double base = sign*ldexp(1.0, exponents[e]);
            double h = ldexp(1.0, exponents[e]-52);
            const double values[] = {base,base+h,base+h};
            xvalue *x = numbers(values, 3), *y = numbers(dependent, 3);
            if (!x || !y) { xrtValueRelease(x); xrtValueRelease(y); return 2; }
            compare("variance", xstat_variance(x), h*(h*(2.0/9.0)));
            compare("varianceSample", xstat_variance_sample(x), h*(h/3.0));
            compare("stdev", xstat_stdev(x), h*(sqrt(2.0)/3.0));
            compare("stdevSample", xstat_stdev_sample(x), h/sqrt(3.0));
            compare("mad", xstat_mad(x), h*(4.0/9.0));
            compare("covariance", xstat_covariance(x,y), h*(2.0/9.0));
            compare("covarianceSample", xstat_covariance_sample(x,y), h/3.0);
            compare("correlation", xstat_correlation(x,y), 1);
            xvalue *fit = xstat_linear_fit(x,y);
            const xvalue *slope = fit ? xrtValueArrayGet(fit,0) : NULL;
            const xvalue *intercept = fit ? xrtValueArrayGet(fit,1) : NULL;
            compare("slope", slope ? xrtValueRetain((xvalue*)slope) : NULL, 1.0/h);
            compare("intercept", intercept ? xrtValueRetain((xvalue*)intercept) : NULL, -base/h);
            xrtValueRelease(fit); xrtValueRelease(x); xrtValueRelease(y);
        }
    }
    return 0;
}

static void bits(double number) {
    uint64_t representation;
    memcpy(&representation,&number,sizeof(representation));
    printf("%016" PRIx64, representation);
}

static int record(xvalue *value) {
    double actual;
    if (!value || xrtGetError() || !xrtValueGetFloat(value,&actual)) {
        xrtValueRelease(value); return 0;
    }
    putchar('\t'); bits(actual); xrtValueRelease(value); return 1;
}

/* 保存实际binary64输入和输出，Fraction可恢复精确有理数，不借用产品均值。 */
static int records(void) {
    static const int exponents[] = {-440,-100,0,26,52,100,400,540};
    uint32_t seed = 0x8e17b3d9u;
    for (unsigned trial = 0; trial < 1024; ++trial) {
        double ax[10], ay[10];
        size_t count = 3+trial%8;
        int ex = exponents[trial%8], ey = exponents[(trial/8)%8];
        double bx = (trial&64 ? -1 : 1)*ldexp(1,ex);
        double by = (trial&128 ? -1 : 1)*ldexp(1,ey);
        double hx = ldexp(1,ex-48-(int)(trial%5));
        double hy = ldexp(1,ey-48-(int)((trial/5)%5));
        for (size_t i = 0; i < count; ++i) {
            seed = seed*1664525u+1013904223u;
            int vx = i < 2 ? (int)i : (int)((seed>>16)%17)-8;
            seed = seed*1664525u+1013904223u;
            int vy = i < 2 ? (int)i : (int)((seed>>16)%17)-8;
            ax[i] = bx+vx*hx; ay[i] = by+vy*hy;
        }
        xvalue *x = numbers(ax,count), *y = numbers(ay,count), *fit = NULL;
        int ok = x && y;
        if (ok) {
            printf("%u\t",trial);
            for (size_t i = 0; i < count; ++i) { if (i) putchar(','); bits(ax[i]); }
            putchar('\t');
            for (size_t i = 0; i < count; ++i) { if (i) putchar(','); bits(ay[i]); }
            ok = record(xstat_variance(x)) && record(xstat_variance_sample(x)) &&
                record(xstat_stdev(x)) && record(xstat_stdev_sample(x)) && record(xstat_mad(x)) &&
                record(xstat_covariance(x,y)) && record(xstat_covariance_sample(x,y)) &&
                record(xstat_correlation(x,y));
            fit = xstat_linear_fit(x,y);
            if (!fit || xrtValueCount(fit) != 2) ok = 0;
            if (ok) ok = record(xrtValueRetain(xrtValueArrayGet(fit,0))) &&
                record(xrtValueRetain(xrtValueArrayGet(fit,1)));
            putchar('\n');
        }
        xrtValueRelease(fit); xrtValueRelease(x); xrtValueRelease(y);
        if (!ok) return 2;
    }
    return 0;
}

int main(int argc,char **argv) {
    xmemdebugsnapshot before,after;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    int status = argc == 1 ? fixed_cases() : argc == 2 && strcmp(argv[1],"--records")==0 ? records() : 2;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
        before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount ||
        before.OverflowCount != after.OverflowCount || before.UnderflowCount != after.UnderflowCount) return 3;
    if (argc == 1) printf("statistics translation: %u checks, %u failures\n",checks,failures);
    return status ? status : failures ? 1 : 0;
}
