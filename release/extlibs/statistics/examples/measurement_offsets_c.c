#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../statistics_native.h"
#include <math.h>
#include <stdio.h>

#ifdef __TINYC__
#include "../statistics_native.c"
#endif

static int near(xvalue *value, double expected) {
    double actual;
    int ok = value && !xrtGetError() && xrtValueGetFloat(value,&actual) &&
        fabs(actual-expected) < 1e-12;
    xrtValueRelease(value);
    return ok;
}

int main(void) {
    const double readings[] = {1e16,1e16+2,1e16+2}, response[] = {0,1,1};
    xvalue *x = xrtValueArray(), *y = xrtValueArray(), *fit = NULL;
    double slope,intercept,original;
    int status = 1;
    if (!x || !y) goto done;
    for (size_t i = 0; i < 3; ++i) {
        if (!xrtValueArrayAppendNew(x,xrtValueFloat(readings[i])) ||
            !xrtValueArrayAppendNew(y,xrtValueFloat(response[i]))) goto done;
    }
    if (!near(xstat_variance(x),8.0/9.0)) goto done;
    puts("variance: 8/9");
    if (!near(xstat_variance_sample(x),4.0/3.0)) goto done;
    puts("sample variance: 4/3");
    if (!near(xstat_stdev(x),sqrt(8.0)/3.0)) goto done;
    puts("stdev: sqrt(8)/3");
    if (!near(xstat_mad(x),8.0/9.0)) goto done;
    puts("MAD: 8/9");
    fit = xstat_linear_fit(x,y);
    if (!fit || xrtGetError() || !xrtValueGetFloat(xrtValueArrayGet(fit,0),&slope) ||
        !xrtValueGetFloat(xrtValueArrayGet(fit,1),&intercept) ||
        fabs(slope-0.5) >= 1e-12 || fabs(intercept+5e15) >= 1 ||
        !near(xstat_correlation(x,y),1)) goto done;
    puts("trend: slope=0.5 correlation=1");
    if (!xrtValueGetFloat(xrtValueArrayGet(x,0),&original) || original != readings[0] ||
        !xrtValueGetFloat(xrtValueArrayGet(x,1),&original) || original != readings[1]) goto done;
    puts("statistics offset example: PASS");
    status = 0;
done:
    xrtValueRelease(fit); xrtValueRelease(x); xrtValueRelease(y);
    return status;
}
