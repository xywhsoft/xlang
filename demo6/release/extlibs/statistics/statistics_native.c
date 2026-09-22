/*
 * statistics_native.c - Descriptive statistics over xlang numeric arrays.
 *
 * Input crosses the ABI as an xvalue array (xlang var). Every aggregate
 * validates that all elements are numeric; on invalid or empty input the
 * function reports an XRT error and returns null so the xlang wrapper can
 * raise a language exception. Percentiles use linear interpolation between
 * closest ranks (the numpy "linear" method).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XSTAT_DOMAIN "xlang.extlib.statistics"

static void xstat_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSTAT_DOMAIN, 0, message);
}

static int xstat_compare(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

/* Coerce any numeric xvalue (int or float) to double. GetFloat only
   accepts true float storage, so ints go through GetInt. */
static int xstat_as_double(xvalue *item, double *out) {
    int64 i;
    if (xrtValueGetFloat(item, out)) {
        return 1;
    }
    if (xrtValueGetInt(item, &i)) {
        *out = (double)i;
        return 1;
    }
    return 0;
}

/* Collect array elements into a malloc'd double buffer. Returns 0 and sets
   the XRT error when the input is not a numeric array. */
static int xstat_collect(xvalue *values, double **out, size_t *count) {
    int64 i;
    int64 n;
    double *buf;

    *out = NULL;
    *count = 0;
    if (values == NULL) {
        xstat_error("input is not an array");
        return 0;
    }
    n = xrtValueCount(values);
    if (n <= 0) {
        xstat_error("input array is empty");
        return 0;
    }
    buf = (double *)xrtMalloc((size_t)n * sizeof(double));
    if (buf == NULL) {
        xstat_error("out of memory");
        return 0;
    }
    for (i = 0; i < n; ++i) {
        xvalue *item = xrtValueArrayAt(values, i);
        double d;
        if (item == NULL || !xrtValueIsNumber(item) ||
            !xstat_as_double(item, &d) || !isfinite(d)) {
            xrtFree(buf);
            xstat_error("all elements must be finite numbers");
            return 0;
        }
        buf[i] = d;
    }
    *out = buf;
    *count = (size_t)n;
    return 1;
}

static double xstat_mean_of(const double *a, size_t n) {
    double sum = 0;
    size_t i;
    for (i = 0; i < n; ++i) sum += a[i];
    return sum / (double)n;
}

static double xstat_percentile_sorted(const double *sorted, size_t n, double p) {
    double rank;
    double frac;
    size_t lo;
    size_t hi;
    if (n == 1) return sorted[0];
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    rank = p / 100.0 * (double)(n - 1);
    lo = (size_t)floor(rank);
    hi = (size_t)ceil(rank);
    if (lo >= n) lo = n - 1;
    if (hi >= n) hi = n - 1;
    frac = rank - (double)lo;
    return sorted[lo] + frac * (sorted[hi] - sorted[lo]);
}

static xvalue *xstat_fail(const char *message) {
    xstat_error(message);
    return xrtValueNull();
}

long long xstat_count(xvalue *values) {
    int64 n;
    int64 i;
    long long numeric = 0;
    if (values == NULL) return 0;
    n = xrtValueCount(values);
    for (i = 0; i < n; ++i) {
        xvalue *item = xrtValueArrayAt(values, i);
        if (item != NULL && xrtValueIsNumber(item)) {
            numeric++;
        }
    }
    return numeric;
}

xvalue *xstat_sum(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double sum = 0;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    for (i = 0; i < n; ++i) sum += a[i];
    xrtFree(a);
    return xrtValueFloat(sum);
}

xvalue *xstat_mean(xvalue *values) {
    double *a;
    size_t n;
    double mean;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    mean = xstat_mean_of(a, n);
    xrtFree(a);
    return xrtValueFloat(mean);
}

xvalue *xstat_median(xvalue *values) {
    double *a;
    size_t n;
    double median;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    qsort(a, n, sizeof(double), xstat_compare);
    median = xstat_percentile_sorted(a, n, 50.0);
    xrtFree(a);
    return xrtValueFloat(median);
}

xvalue *xstat_mode(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    size_t best_start = 0;
    size_t best_len = 0;
    size_t run_start = 0;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    qsort(a, n, sizeof(double), xstat_compare);
    for (i = 1; i <= n; ++i) {
        if (i == n || a[i] != a[run_start]) {
            size_t run_len = i - run_start;
            if (run_len > best_len) {
                best_len = run_len;
                best_start = run_start;
            }
            run_start = i;
        }
    }
    {
        xvalue *result = xrtValueFloat(a[best_start]);
        xrtFree(a);
        return result;
    }
}

xvalue *xstat_min(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double m;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    m = a[0];
    for (i = 1; i < n; ++i) {
        if (a[i] < m) m = a[i];
    }
    xrtFree(a);
    return xrtValueFloat(m);
}

xvalue *xstat_max(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double m;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    m = a[0];
    for (i = 1; i < n; ++i) {
        if (a[i] > m) m = a[i];
    }
    xrtFree(a);
    return xrtValueFloat(m);
}

xvalue *xstat_range(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double lo;
    double hi;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    lo = hi = a[0];
    for (i = 1; i < n; ++i) {
        if (a[i] < lo) lo = a[i];
        if (a[i] > hi) hi = a[i];
    }
    xrtFree(a);
    return xrtValueFloat(hi - lo);
}

static xvalue *xstat_variance_impl(xvalue *values, int sample) {
    double *a;
    size_t n;
    size_t i;
    double mean;
    double acc = 0;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    if (sample && n < 2) {
        xrtFree(a);
        return xstat_fail("sample variance requires at least two values");
    }
    mean = xstat_mean_of(a, n);
    for (i = 0; i < n; ++i) {
        double d = a[i] - mean;
        acc += d * d;
    }
    xrtFree(a);
    return xrtValueFloat(acc / (double)(sample ? n - 1 : n));
}

xvalue *xstat_variance(xvalue *values) {
    return xstat_variance_impl(values, 0);
}

xvalue *xstat_variance_sample(xvalue *values) {
    return xstat_variance_impl(values, 1);
}

static xvalue *xstat_stdev_impl(xvalue *values, int sample) {
    xvalue *v = xstat_variance_impl(values, sample);
    double d;
    if (v == NULL || !xrtValueGetFloat(v, &d)) {
        return xrtValueNull();
    }
    return xrtValueFloat(sqrt(d));
}

xvalue *xstat_stdev(xvalue *values) {
    return xstat_stdev_impl(values, 0);
}

xvalue *xstat_stdev_sample(xvalue *values) {
    return xstat_stdev_impl(values, 1);
}

xvalue *xstat_percentile(xvalue *values, double p) {
    double *a;
    size_t n;
    double result;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    qsort(a, n, sizeof(double), xstat_compare);
    result = xstat_percentile_sorted(a, n, p);
    xrtFree(a);
    return xrtValueFloat(result);
}

xvalue *xstat_quartiles(xvalue *values) {
    double *a;
    size_t n;
    double q1;
    double q2;
    double q3;
    xvalue *result;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    qsort(a, n, sizeof(double), xstat_compare);
    q1 = xstat_percentile_sorted(a, n, 25.0);
    q2 = xstat_percentile_sorted(a, n, 50.0);
    q3 = xstat_percentile_sorted(a, n, 75.0);
    xrtFree(a);
    result = xrtValueArray();
    if (result == NULL) return xrtValueNull();
    if (!xrtValueArrayAppendNew(result, xrtValueFloat(q1)) ||
        !xrtValueArrayAppendNew(result, xrtValueFloat(q2)) ||
        !xrtValueArrayAppendNew(result, xrtValueFloat(q3))) {
        xrtValueRelease(result);
        return xrtValueNull();
    }
    return result;
}

xvalue *xstat_iqr(xvalue *values) {
    double *a;
    size_t n;
    double q1;
    double q3;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    qsort(a, n, sizeof(double), xstat_compare);
    q1 = xstat_percentile_sorted(a, n, 25.0);
    q3 = xstat_percentile_sorted(a, n, 75.0);
    xrtFree(a);
    return xrtValueFloat(q3 - q1);
}

xvalue *xstat_mad(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double mean;
    double acc = 0;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    mean = xstat_mean_of(a, n);
    for (i = 0; i < n; ++i) {
        acc += fabs(a[i] - mean);
    }
    xrtFree(a);
    return xrtValueFloat(acc / (double)n);
}

xvalue *xstat_geomean(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double acc = 0;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    for (i = 0; i < n; ++i) {
        if (a[i] <= 0) {
            xrtFree(a);
            return xstat_fail("geometric mean requires positive values");
        }
        acc += log(a[i]);
    }
    xrtFree(a);
    return xrtValueFloat(exp(acc / (double)n));
}

xvalue *xstat_harmmean(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double acc = 0;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    for (i = 0; i < n; ++i) {
        if (a[i] == 0) {
            xrtFree(a);
            return xstat_fail("harmonic mean requires non-zero values");
        }
        acc += 1.0 / a[i];
    }
    xrtFree(a);
    return xrtValueFloat((double)n / acc);
}

static int xstat_collect_pair(xvalue *x, xvalue *y, double **ax, double **ay,
    size_t *count) {
    int64 nx;
    int64 ny;
    int64 i;
    double *bx;
    double *by;
    if (x == NULL || y == NULL) {
        xstat_error("paired input requires two arrays");
        return 0;
    }
    nx = xrtValueCount(x);
    ny = xrtValueCount(y);
    if (nx <= 0 || ny <= 0) {
        xstat_error("paired input arrays are empty");
        return 0;
    }
    if (nx != ny) {
        xstat_error("paired input arrays must have the same length");
        return 0;
    }
    bx = (double *)xrtMalloc((size_t)nx * sizeof(double));
    by = (double *)xrtMalloc((size_t)nx * sizeof(double));
    if (bx == NULL || by == NULL) {
        xstat_error("out of memory");
        xrtFree(bx);
        xrtFree(by);
        return 0;
    }
    for (i = 0; i < nx; ++i) {
        xvalue *ix = xrtValueArrayAt(x, i);
        xvalue *iy = xrtValueArrayAt(y, i);
        double dx;
        double dy;
        if (ix == NULL || iy == NULL || !xrtValueIsNumber(ix) ||
            !xrtValueIsNumber(iy) || !xstat_as_double(ix, &dx) ||
            !xstat_as_double(iy, &dy) || !isfinite(dx) || !isfinite(dy)) {
            xstat_error("all paired elements must be finite numbers");
            xrtFree(bx);
            xrtFree(by);
            return 0;
        }
        bx[i] = dx;
        by[i] = dy;
    }
    *ax = bx;
    *ay = by;
    *count = (size_t)nx;
    return 1;
}

static void xstat_sums(const double *a, size_t n, double *mean, double *varacc) {
    double m = xstat_mean_of(a, n);
    double acc = 0;
    size_t i;
    for (i = 0; i < n; ++i) {
        double d = a[i] - m;
        acc += d * d;
    }
    *mean = m;
    *varacc = acc;
}

static xvalue *xstat_covariance_impl(xvalue *x, xvalue *y, int sample) {
    double *ax;
    double *ay;
    size_t n;
    size_t i;
    double mx;
    double my;
    double vx;
    double vy;
    double acc = 0;
    if (!xstat_collect_pair(x, y, &ax, &ay, &n)) return xrtValueNull();
    if (sample && n < 2) {
        xrtFree(ax);
        xrtFree(ay);
        return xstat_fail("sample covariance requires at least two pairs");
    }
    xstat_sums(ax, n, &mx, &vx);
    xstat_sums(ay, n, &my, &vy);
    for (i = 0; i < n; ++i) {
        acc += (ax[i] - mx) * (ay[i] - my);
    }
    xrtFree(ax);
    xrtFree(ay);
    return xrtValueFloat(acc / (double)(sample ? n - 1 : n));
}

xvalue *xstat_covariance(xvalue *x, xvalue *y) {
    return xstat_covariance_impl(x, y, 0);
}

xvalue *xstat_covariance_sample(xvalue *x, xvalue *y) {
    return xstat_covariance_impl(x, y, 1);
}

xvalue *xstat_correlation(xvalue *x, xvalue *y) {
    double *ax;
    double *ay;
    size_t n;
    size_t i;
    double mx;
    double my;
    double vx;
    double vy;
    double acc = 0;
    double denom;
    if (!xstat_collect_pair(x, y, &ax, &ay, &n)) return xrtValueNull();
    xstat_sums(ax, n, &mx, &vx);
    xstat_sums(ay, n, &my, &vy);
    for (i = 0; i < n; ++i) {
        acc += (ax[i] - mx) * (ay[i] - my);
    }
    denom = sqrt(vx) * sqrt(vy);
    xrtFree(ax);
    xrtFree(ay);
    if (denom <= 0) {
        return xstat_fail("correlation requires non-constant inputs");
    }
    return xrtValueFloat(acc / denom);
}

xvalue *xstat_linear_fit(xvalue *x, xvalue *y) {
    double *ax;
    double *ay;
    size_t n;
    size_t i;
    double mx;
    double my;
    double vx;
    double vy;
    double acc = 0;
    double slope;
    double intercept;
    xvalue *result;
    if (!xstat_collect_pair(x, y, &ax, &ay, &n)) return xrtValueNull();
    xstat_sums(ax, n, &mx, &vx);
    xstat_sums(ay, n, &my, &vy);
    for (i = 0; i < n; ++i) {
        acc += (ax[i] - mx) * (ay[i] - my);
    }
    if (vx <= 0) {
        xrtFree(ax);
        xrtFree(ay);
        return xstat_fail("linear fit requires non-constant x values");
    }
    slope = acc / vx;
    intercept = my - slope * mx;
    xrtFree(ax);
    xrtFree(ay);
    result = xrtValueArray();
    if (result == NULL) return xrtValueNull();
    if (!xrtValueArrayAppendNew(result, xrtValueFloat(slope)) ||
        !xrtValueArrayAppendNew(result, xrtValueFloat(intercept))) {
        xrtValueRelease(result);
        return xrtValueNull();
    }
    return result;
}

xvalue *xstat_histogram(xvalue *values, long long bins) {
    double *a;
    size_t n;
    size_t i;
    long long b;
    double lo;
    double hi;
    double span;
    xvalue *result;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    if (bins < 1) {
        xrtFree(a);
        return xstat_fail("histogram requires at least one bin");
    }
    lo = hi = a[0];
    for (i = 1; i < n; ++i) {
        if (a[i] < lo) lo = a[i];
        if (a[i] > hi) hi = a[i];
    }
    span = hi - lo;
    result = xrtValueArray();
    if (result == NULL) {
        xrtFree(a);
        return xrtValueNull();
    }
    for (b = 0; b < bins; ++b) {
        if (!xrtValueArrayAppendNew(result, xrtValueInt(0))) {
            xrtValueRelease(result);
            xrtFree(a);
            return xrtValueNull();
        }
    }
    for (i = 0; i < n; ++i) {
        long long bin;
        if (span == 0) {
            bin = 0;
        } else {
            bin = (long long)floor((a[i] - lo) / span * (double)bins);
            if (bin >= bins) bin = bins - 1;
            if (bin < 0) bin = 0;
        }
        {
            xvalue *slot = xrtValueArrayAt(result, (int64)bin);
            double current = 0;
            if (slot != NULL && !xstat_as_double(slot, &current)) {
                current = 0;
            }
            if (!xrtValueArraySetNew(result, (int64)bin, xrtValueInt((int64)current + 1))) {
                xrtValueRelease(result);
                xrtFree(a);
                return xrtValueNull();
            }
        }
    }
    xrtFree(a);
    return result;
}
