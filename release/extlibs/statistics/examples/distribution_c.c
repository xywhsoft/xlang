#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../statistics_native.h"
#include <stdio.h>

static xvalue *make_array(const int *values, size_t count)
{
    xvalue *array = xrtValueArray();
    size_t i;
    if (array == NULL) return NULL;
    for (i = 0; i < count; ++i) {
        if (!xrtValueArrayAppendNew(array, xrtValueInt(values[i]))) {
            xrtValueRelease(array);
            return NULL;
        }
    }
    return array;
}

static int show(const char *name, xvalue *result, int decimals)
{
    double value;
    int valid = result != NULL && xrtValueType(result) == XVALUE_FLOAT &&
        xrtValueGetFloat(result, &value);
    if (valid) printf("%s: %.*f\n", name, decimals, value);
    xrtValueRelease(result);
    return valid;
}

int main(void)
{
    const int score_values[] = {4, 4, 6, 6};
    const int factor_values[] = {1, 4, 4, 1};
    const int period_values[] = {1, 2, 3};
    const int sale_values[] = {2, 4, 6};
    xvalue *scores = make_array(score_values, 4);
    xvalue *factors = make_array(factor_values, 4);
    xvalue *periods = make_array(period_values, 3);
    xvalue *sales = make_array(sale_values, 3);
    int ok = 0;

    if (scores == NULL || factors == NULL || periods == NULL || sales == NULL)
        goto done;
    if (!show("mode", xstat_mode(scores), 1) ||
        !show("variance", xstat_variance(scores), 1) ||
        !show("varianceSample", xstat_variance_sample(scores), 2) ||
        !show("stdevSample", xstat_stdev_sample(scores), 2) ||
        !show("mad", xstat_mad(scores), 1) ||
        !show("iqr", xstat_iqr(scores), 1) ||
        !show("percentile25", xstat_percentile(scores, 25), 1) ||
        !show("geomean", xstat_geomean(factors), 1) ||
        !show("harmmean", xstat_harmmean(factors), 1) ||
        !show("covariance", xstat_covariance(periods, sales), 2) ||
        !show("covarianceSample", xstat_covariance_sample(periods, sales), 1))
        goto done;
    ok = 1;
done:
    xrtValueRelease(scores);
    xrtValueRelease(factors);
    xrtValueRelease(periods);
    xrtValueRelease(sales);
    return ok ? 0 : 1;
}

/* 范例自包含，支持直接交给 xl run 执行。 */
#include "../statistics_native.c"
