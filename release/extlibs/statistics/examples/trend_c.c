#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../statistics_native.h"
#include <inttypes.h>
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

int main(void)
{
    const int period_values[] = {1, 2, 3, 4, 5};
    const int sale_values[] = {3, 5, 7, 9, 11};
    xvalue *periods = make_array(period_values, 5);
    xvalue *sales = make_array(sale_values, 5);
    xvalue *quartiles = NULL, *fit = NULL, *correlation = NULL, *buckets = NULL;
    double q1, median, q3, slope, intercept, r;
    int64_t counts[4];
    int ok = 0;
    size_t i;

    if (periods == NULL || sales == NULL) goto done;
    quartiles = xstat_quartiles(sales);
    fit = xstat_linear_fit(periods, sales);
    correlation = xstat_correlation(periods, sales);
    buckets = xstat_histogram(sales, 4);
    if (quartiles == NULL || fit == NULL || correlation == NULL || buckets == NULL ||
        xrtValueCount(quartiles) != 3 || xrtValueCount(fit) != 2 ||
        xrtValueCount(buckets) != 4 ||
        !xrtValueGetFloat(xrtValueArrayAt(quartiles, 0), &q1) ||
        !xrtValueGetFloat(xrtValueArrayAt(quartiles, 1), &median) ||
        !xrtValueGetFloat(xrtValueArrayAt(quartiles, 2), &q3) ||
        !xrtValueGetFloat(xrtValueArrayAt(fit, 0), &slope) ||
        !xrtValueGetFloat(xrtValueArrayAt(fit, 1), &intercept) ||
        !xrtValueGetFloat(correlation, &r)) goto done;
    for (i = 0; i < 4; ++i) {
        if (!xrtValueGetInt(xrtValueArrayAt(buckets, (int64_t)i), &counts[i]))
            goto done;
    }

    printf("quartiles: %.1f %.1f %.1f\n", q1, median, q3);
    printf("linearFit: %.1f %.1f\n", slope, intercept);
    printf("correlation: %.1f\n", r);
    printf("histogram: %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n",
        counts[0], counts[1], counts[2], counts[3]);
    ok = 1;
done:
    xrtValueRelease(quartiles);
    xrtValueRelease(fit);
    xrtValueRelease(correlation);
    xrtValueRelease(buckets);
    xrtValueRelease(periods);
    xrtValueRelease(sales);
    return ok ? 0 : 1;
}

/* 示例自包含：xl run 只接收当前 C 文件，统计实现随此编译单元进入。 */
#include "../statistics_native.c"
