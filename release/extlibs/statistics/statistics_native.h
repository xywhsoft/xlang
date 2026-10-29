#ifndef XL_STATISTICS_NATIVE_H
#define XL_STATISTICS_NATIVE_H

#include <stdint.h>
typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 除 count 外，结果为 XRT xvalue；C 调用方用 xrtValueRelease 释放。 */
int64_t xstat_count(xvalue *values);
xvalue *xstat_sum(xvalue *values);
xvalue *xstat_mean(xvalue *values);
xvalue *xstat_median(xvalue *values);
xvalue *xstat_mode(xvalue *values);
xvalue *xstat_min(xvalue *values);
xvalue *xstat_max(xvalue *values);
xvalue *xstat_range(xvalue *values);
xvalue *xstat_variance(xvalue *values);
xvalue *xstat_variance_sample(xvalue *values);
xvalue *xstat_stdev(xvalue *values);
xvalue *xstat_stdev_sample(xvalue *values);
xvalue *xstat_percentile(xvalue *values, double percentile);
xvalue *xstat_quartiles(xvalue *values);
xvalue *xstat_iqr(xvalue *values);
xvalue *xstat_mad(xvalue *values);
xvalue *xstat_geomean(xvalue *values);
xvalue *xstat_harmmean(xvalue *values);
xvalue *xstat_covariance(xvalue *x, xvalue *y);
xvalue *xstat_covariance_sample(xvalue *x, xvalue *y);
xvalue *xstat_correlation(xvalue *x, xvalue *y);
xvalue *xstat_linear_fit(xvalue *x, xvalue *y);
xvalue *xstat_histogram(xvalue *values, int64_t bins);

#ifdef __cplusplus
}
#endif
#endif
