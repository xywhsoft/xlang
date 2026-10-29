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
#include <float.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "statistics_native.h"

#define XSTAT_DOMAIN "xlang.extlib.statistics"

static void xstat_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSTAT_DOMAIN, 0, message);
}

static void xstat_memory(void) {
    xrtSetErrorInfo(XERR_MEMORY, XSTAT_DOMAIN, 0, "out of memory");
}

/* 分配前计算字节数，避免 32 位平台的大数组乘法回绕。 */
static int xstat_double_bytes(int64 count, size_t *bytes) {
    if ((uint64_t)count > SIZE_MAX / sizeof(double)) {
        xrtSetErrorInfo(XERR_RANGE, XSTAT_DOMAIN, 0,
            "numeric array exceeds addressable range");
        return 0;
    }
    *bytes = (size_t)count * sizeof(double);
    return 1;
}

static int xstat_compare(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

/* XRT 的 Get 接口是精确读取；先判型，避免探测失败污染错误状态。 */
static int xstat_as_double(xvalue *item, double *out) {
    int64 i;
    uint64 u;
    uint32 c;
    switch (xrtValueType(item)) {
        case XVALUE_FLOAT:
            return xrtValueGetFloat(item, out);
        case XVALUE_INT:
            if (!xrtValueGetInt(item, &i)) return 0;
            *out = (double)i;
            return 1;
        case XVALUE_UINT:
            if (!xrtValueGetUInt(item, &u)) return 0;
            *out = (double)u;
            return 1;
        case XVALUE_CHAR:
            if (!xrtValueGetChar(item, &c)) return 0;
            *out = (double)c;
            return 1;
        default:
            return 0;
    }
}

/* Collect array elements into a malloc'd double buffer. Returns 0 and sets
   the XRT error when the input is not a numeric array. */
static int xstat_collect(xvalue *values, double **out, size_t *count) {
    int64 i;
    int64 n;
    double *buf;
    size_t bytes;

    xrtClearError();
    *out = NULL;
    *count = 0;
    if (values == NULL || xrtValueType(values) != XVALUE_ARRAY) {
        xstat_error("input is not an array");
        return 0;
    }
    n = xrtValueCount(values);
    if (n <= 0) {
        xstat_error("input array is empty");
        return 0;
    }
    if (!xstat_double_bytes(n, &bytes)) return 0;
    buf = (double *)xrtMalloc(bytes);
    if (buf == NULL) {
        xstat_memory();
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

static int xstat_can_sum_directly(const double *a, size_t n) {
    double limit = DBL_MAX / (double)n;
    size_t i;
    for (i = 0; i < n; ++i)
        if (fabs(a[i]) >= limit) return 0;
    return 1;
}

static void xstat_add_compensated(double value, double *sum, double *correction) {
    double next = *sum + value;
    *correction += fabs(*sum) >= fabs(value) ?
        (*sum - next) + value : (value - next) + *sum;
    *sum = next;
}

static double xstat_sum_compensated(const double *a, size_t n) {
    double sum = 0, correction = 0;
    size_t i;
    /* Neumaier 补偿保留大数抵消时被舍入掉的小项。 */
    for (i = 0; i < n; ++i)
        xstat_add_compensated(a[i], &sum, &correction);
    return sum + correction;
}

/* 分开保留缩放后的小项，避免极大数抵消时丢失次正规余量。 */
static void xstat_sum_scaled(const double *a, size_t n,
    double *large, double *small, double *scale) {
    double sum = 0, correction = 0;
    double tiny = 0, tinyCorrection = 0;
    size_t i;
    *scale = 1.0;
    while (*scale < (double)n) *scale *= 2.0;
    for (i = 0; i < n; ++i) {
        if (fabs(a[i]) < DBL_MIN * *scale)
            xstat_add_compensated(a[i], &tiny, &tinyCorrection);
        else
            xstat_add_compensated(a[i] / *scale, &sum, &correction);
    }
    *large = sum + correction;
    *small = tiny + tinyCorrection;
}

static double xstat_mean_of(const double *a, size_t n) {
    double large, small, scale, mean;
    double minimum = a[0], maximum = a[0];
    size_t i;
    /* 安全时先求和，避免次正规数逐项除以 n 后全部下溢。 */
    if (xstat_can_sum_directly(a, n))
        return xstat_sum_compensated(a, n) / (double)n;
    /* 先缩放累加再求均值，不形成可能溢出的总和。 */
    xstat_sum_scaled(a, n, &large, &small, &scale);
    mean = large * (scale / (double)n) + small / (double)n;
    /* 极端舍入可能越过最大有限输入，均值必须落在输入范围内。 */
    for (i = 1; i < n; ++i) {
        if (a[i] < minimum) minimum = a[i];
        if (a[i] > maximum) maximum = a[i];
    }
    if (mean < minimum) return minimum;
    if (mean > maximum) return maximum;
    return mean;
}

static double xstat_percentile_sorted(const double *sorted, size_t n, double p) {
    double delta;
    double rank;
    double frac;
    size_t lo;
    size_t hi;
    if (n == 1) return sorted[0];
    rank = p / 100.0 * (double)(n - 1);
    lo = (size_t)floor(rank);
    hi = (size_t)ceil(rank);
    if (lo >= n) lo = n - 1;
    if (hi >= n) hi = n - 1;
    frac = rank - (double)lo;
    delta = sorted[hi] - sorted[lo];
    /* 极小数先按 2 的幂精确放大，避免中间乘积提前下溢。 */
    if (fabs(sorted[lo]) < 2.0 * DBL_MIN &&
        fabs(sorted[hi]) < 2.0 * DBL_MIN) {
        double left = ldexp(sorted[lo], 54);
        double right = ldexp(sorted[hi], 54);
        return ldexp(left + (right - left) * frac, -54);
    }
    if (isfinite(delta)) return sorted[lo] + delta * frac;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

static xvalue *xstat_fail(const char *message) {
    xstat_error(message);
    return xrtValueNull();
}

static xvalue *xstat_number(double value) {
    xvalue *result;
    if (!isfinite(value)) return xstat_fail("result exceeds finite float range");
    result = xrtValueFloat(value);
    return result != NULL ? result : xrtValueNull();
}

int64_t xstat_count(xvalue *values) {
    int64 n;
    int64 i;
    long long numeric = 0;
    xrtClearError();
    if (values == NULL || xrtValueType(values) != XVALUE_ARRAY) return 0;
    n = xrtValueCount(values);
    for (i = 0; i < n; ++i) {
        xvalue *item = xrtValueArrayAt(values, i);
        double value;
        if (item != NULL && xrtValueIsNumber(item) &&
            xstat_as_double(item, &value) && isfinite(value)) {
            numeric++;
        }
    }
    return numeric;
}

xvalue *xstat_sum(xvalue *values) {
    double *a;
    size_t n;
    double sum, small, scale;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    if (xstat_can_sum_directly(a, n)) {
        sum = xstat_sum_compensated(a, n);
        xrtFree(a);
        return xstat_number(sum);
    }
    xstat_sum_scaled(a, n, &sum, &small, &scale);
    sum = sum * scale + small;
    xrtFree(a);
    return xstat_number(sum);
}

xvalue *xstat_mean(xvalue *values) {
    double *a;
    size_t n;
    double mean;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    mean = xstat_mean_of(a, n);
    xrtFree(a);
    return xstat_number(mean);
}

xvalue *xstat_median(xvalue *values) {
    double *a;
    size_t n;
    double median;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    qsort(a, n, sizeof(double), xstat_compare);
    median = xstat_percentile_sorted(a, n, 50.0);
    xrtFree(a);
    return xstat_number(median);
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
        xvalue *result = xstat_number(a[best_start]);
        xrtFree(a);
        return result;
    }
}

/* 极值只需扫描，不为整份输入再分配 double 数组。 */
static int xstat_extrema(xvalue *values, double *lo, double *hi) {
    int64 n;
    int64 i;
    xrtClearError();
    if (values == NULL || xrtValueType(values) != XVALUE_ARRAY) {
        xstat_error("input is not an array");
        return 0;
    }
    n = xrtValueCount(values);
    if (n <= 0) {
        xstat_error("input array is empty");
        return 0;
    }
    for (i = 0; i < n; ++i) {
        xvalue *item = xrtValueArrayAt(values, i);
        double value;
        if (item == NULL || !xrtValueIsNumber(item) ||
            !xstat_as_double(item, &value) || !isfinite(value)) {
            xstat_error("all elements must be finite numbers");
            return 0;
        }
        if (i == 0) *lo = *hi = value;
        else {
            if (value < *lo) *lo = value;
            if (value > *hi) *hi = value;
        }
    }
    return 1;
}

xvalue *xstat_min(xvalue *values) {
    double lo, hi;
    if (!xstat_extrema(values, &lo, &hi)) return xrtValueNull();
    return xstat_number(lo);
}

xvalue *xstat_max(xvalue *values) {
    double lo, hi;
    if (!xstat_extrema(values, &lo, &hi)) return xrtValueNull();
    return xstat_number(hi);
}

xvalue *xstat_range(xvalue *values) {
    double lo, hi;
    if (!xstat_extrema(values, &lo, &hi)) return xrtValueNull();
    return xstat_number(hi - lo);
}

static int xstat_center(double *a, size_t n) {
    double origin = a[0], mean;
    size_t i;
    int halve = 0;
    /* 先在首元素附近求偏差均值，避免大均值舍入吞掉真实的小波动。 */
    for (i = 0; i < n; ++i) {
        if (!isfinite(a[i] - origin)) { halve = 1; break; }
    }
    for (i = 0; i < n; ++i)
        a[i] = halve ? a[i] / 2.0 - origin / 2.0 : a[i] - origin;
    mean = xstat_mean_of(a, n);
    /* 首次平移可能安全，但离偏差均值的距离仍可能超出有限范围。 */
    if (!halve) {
        for (i = 0; i < n; ++i) {
            if (!isfinite(a[i] - mean)) {
                halve = 1;
                mean /= 2.0;
                for (size_t j = 0; j < n; ++j) a[j] /= 2.0;
                break;
            }
        }
    }
    for (i = 0; i < n; ++i) a[i] -= mean;
    return halve;
}

static xvalue *xstat_variance_impl(xvalue *values, int sample) {
    double *a;
    size_t n;
    size_t i;
    double scale = 0;
    double squares = 0;
    double variance;
    int halve;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    if (sample && n < 2) {
        xrtFree(a);
        return xstat_fail("sample variance requires at least two values");
    }
    halve = xstat_center(a, n);
    for (i = 0; i < n; ++i)
        if (fabs(a[i]) > scale) scale = fabs(a[i]);
    if (scale != 0) {
        for (i = 0; i < n; ++i) {
            double normalized = a[i] / scale;
            squares += normalized * normalized;
        }
    }
    variance = scale * (scale * (squares / (double)(sample ? n - 1 : n)));
    if (halve) variance *= 4.0;
    xrtFree(a);
    return xstat_number(variance);
}

xvalue *xstat_variance(xvalue *values) {
    return xstat_variance_impl(values, 0);
}

xvalue *xstat_variance_sample(xvalue *values) {
    return xstat_variance_impl(values, 1);
}

static xvalue *xstat_stdev_impl(xvalue *values, int sample) {
    double *a;
    size_t n;
    size_t i;
    double scale = 0;
    double squares = 0;
    double deviation;
    int halve = 0;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    if (sample && n < 2) {
        xrtFree(a);
        return xstat_fail("sample standard deviation requires at least two values");
    }
    /* 方差可能不可表示，但其平方根仍可能是有限值。 */
    halve = xstat_center(a, n);
    for (i = 0; i < n; ++i) {
        if (fabs(a[i]) > scale) scale = fabs(a[i]);
    }
    if (scale != 0) {
        for (i = 0; i < n; ++i) {
            double normalized = a[i] / scale;
            squares += normalized * normalized;
        }
    }
    deviation = scale * sqrt(squares / (double)(sample ? n - 1 : n));
    if (halve) deviation *= 2.0;
    xrtFree(a);
    return xstat_number(deviation);
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
    if (!isfinite(p) || p < 0 || p > 100) {
        return xstat_fail("percentile must be between 0 and 100");
    }
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    qsort(a, n, sizeof(double), xstat_compare);
    result = xstat_percentile_sorted(a, n, p);
    xrtFree(a);
    return xstat_number(result);
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
    return xstat_number(q3 - q1);
}

xvalue *xstat_mad(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double mean;
    int halve;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    halve = xstat_center(a, n);
    for (i = 0; i < n; ++i) a[i] = fabs(a[i]);
    mean = xstat_mean_of(a, n);
    if (halve) mean *= 2.0;
    xrtFree(a);
    return xstat_number(mean);
}

xvalue *xstat_geomean(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double lo = DBL_MAX, hi = 0;
    double acc = 0, result;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    for (i = 0; i < n; ++i) {
        if (a[i] <= 0) {
            xrtFree(a);
            return xstat_fail("geometric mean requires positive values");
        }
        if (a[i] < lo) lo = a[i];
        if (a[i] > hi) hi = a[i];
        acc += log(a[i]);
    }
    /* 对数累计舍入可能把有限均值推过端点；几何均值必在输入范围内。 */
    result = exp(acc / (double)n);
    if (result < lo) result = lo;
    if (result > hi) result = hi;
    xrtFree(a);
    return xstat_number(result);
}

xvalue *xstat_harmmean(xvalue *values) {
    double *a;
    size_t n;
    size_t i;
    double scale;
    double acc = 0;
    double correction = 0;
    double result;
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    scale = fabs(a[0]);
    for (i = 0; i < n; ++i) {
        if (a[i] == 0) {
            xrtFree(a);
            return xstat_fail("harmonic mean requires non-zero values");
        }
        if (fabs(a[i]) < scale) scale = fabs(a[i]);
    }
    /* 缩放避免倒数溢出；补偿求和保留大项抵消后的小余项。 */
    for (i = 0; i < n; ++i) {
        xstat_add_compensated(scale / a[i], &acc, &correction);
    }
    xrtFree(a);
    acc += correction;
    if (!isfinite(acc) || acc == 0) {
        return xstat_fail("harmonic mean is undefined for these values");
    }
    result = scale * ((double)n / acc);
    if (!isfinite(result)) result = (scale / acc) * (double)n;
    return xstat_number(result);
}

static int xstat_collect_pair(xvalue *x, xvalue *y, double **ax, double **ay,
    size_t *count) {
    int64 nx;
    int64 ny;
    int64 i;
    double *bx;
    double *by;
    size_t bytes;
    xrtClearError();
    if (x == NULL || y == NULL ||
        xrtValueType(x) != XVALUE_ARRAY || xrtValueType(y) != XVALUE_ARRAY) {
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
    if (!xstat_double_bytes(nx, &bytes)) return 0;
    bx = (double *)xrtMalloc(bytes);
    by = (double *)xrtMalloc(bytes);
    if (bx == NULL || by == NULL) {
        xstat_memory();
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

typedef struct {
    double mx, my;
    double sx, sy;
    double vx, vy, cov;
    int hx, hy;
} xstat_pair_moments;

static void xstat_pair_moments_calc(double *ax, double *ay, size_t n,
    xstat_pair_moments *m) {
    size_t i;
    memset(m, 0, sizeof(*m));
    m->mx = xstat_mean_of(ax, n);
    m->my = xstat_mean_of(ay, n);
    /* 两组数据复用同一平移/溢出边界，不另维护一套均值相减规则。 */
    m->hx = xstat_center(ax, n);
    m->hy = xstat_center(ay, n);
    for (i = 0; i < n; ++i) {
        if (fabs(ax[i]) > m->sx) m->sx = fabs(ax[i]);
        if (fabs(ay[i]) > m->sy) m->sy = fabs(ay[i]);
    }
    for (i = 0; i < n; ++i) {
        double dx = m->sx == 0 ? 0 : ax[i] / m->sx;
        double dy = m->sy == 0 ? 0 : ay[i] / m->sy;
        m->vx += dx * dx;
        m->vy += dy * dy;
        m->cov += dx * dy;
    }
}

static double xstat_pair_unscale(double value, const xstat_pair_moments *m) {
    int ex, ey;
    double fx, fy;
    if (m->sx == 0 || m->sy == 0) return 0;
    fx = frexp(m->sx, &ex);
    fy = frexp(m->sy, &ey);
    return ldexp(value * fx * fy, ex + ey + m->hx + m->hy);
}

static xvalue *xstat_covariance_impl(xvalue *x, xvalue *y, int sample) {
    double *ax;
    double *ay;
    size_t n;
    xstat_pair_moments m;
    double value;
    if (!xstat_collect_pair(x, y, &ax, &ay, &n)) return xrtValueNull();
    if (sample && n < 2) {
        xrtFree(ax);
        xrtFree(ay);
        return xstat_fail("sample covariance requires at least two pairs");
    }
    xstat_pair_moments_calc(ax, ay, n, &m);
    value = xstat_pair_unscale(m.cov / (double)(sample ? n - 1 : n), &m);
    xrtFree(ax);
    xrtFree(ay);
    return xstat_number(value);
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
    xstat_pair_moments m;
    double denom;
    if (!xstat_collect_pair(x, y, &ax, &ay, &n)) return xrtValueNull();
    xstat_pair_moments_calc(ax, ay, n, &m);
    denom = sqrt(m.vx) * sqrt(m.vy);
    xrtFree(ax);
    xrtFree(ay);
    if (denom <= 0) {
        return xstat_fail("correlation requires non-constant inputs");
    }
    return xstat_number(fmax(-1.0, fmin(1.0, m.cov / denom)));
}

xvalue *xstat_linear_fit(xvalue *x, xvalue *y) {
    double *ax;
    double *ay;
    size_t n;
    xstat_pair_moments m;
    double fx, fy;
    int ex, ey;
    double slope;
    double intercept;
    xvalue *result;
    if (!xstat_collect_pair(x, y, &ax, &ay, &n)) return xrtValueNull();
    xstat_pair_moments_calc(ax, ay, n, &m);
    if (m.vx <= 0) {
        xrtFree(ax);
        xrtFree(ay);
        return xstat_fail("linear fit requires non-constant x values");
    }
    if (m.sy == 0) {
        slope = 0;
    } else {
        fx = frexp(m.sx, &ex);
        fy = frexp(m.sy, &ey);
        slope = ldexp((m.cov / m.vx) * (fy / fx),
            ey - ex + m.hy - m.hx);
    }
    /* 中间乘积溢出而截距有限时，缩半可避开运行库 fma 的溢出路径。 */
    intercept = fma(-slope, m.mx, m.my);
    if (!isfinite(intercept) && isfinite(slope)) {
        intercept = (m.my / 2.0 - slope * (m.mx / 2.0)) * 2.0;
    }
    xrtFree(ax);
    xrtFree(ay);
    if (!isfinite(slope) || !isfinite(intercept)) {
        return xstat_fail("linear fit exceeds finite float range");
    }
    result = xrtValueArray();
    if (result == NULL) return xrtValueNull();
    if (!xrtValueArrayAppendNew(result, xrtValueFloat(slope)) ||
        !xrtValueArrayAppendNew(result, xrtValueFloat(intercept))) {
        xrtValueRelease(result);
        return xrtValueNull();
    }
    return result;
}

xvalue *xstat_histogram(xvalue *values, int64_t bins) {
    double *a;
    int64_t *counts;
    size_t n;
    size_t i;
    long long b;
    double lo;
    double hi;
    double span;
    xvalue *result;
    if (bins < 1 || bins > 100000) {
        xrtClearError();
        return xstat_fail("histogram bin count must be between 1 and 100000");
    }
    if (!xstat_collect(values, &a, &n)) return xrtValueNull();
    lo = hi = a[0];
    for (i = 1; i < n; ++i) {
        if (a[i] < lo) lo = a[i];
        if (a[i] > hi) hi = a[i];
    }
    span = hi - lo;
    counts = (int64_t *)xrtCalloc((size_t)bins, sizeof(*counts));
    if (counts == NULL) {
        xrtFree(a);
        return xrtValueNull();
    }
    for (i = 0; i < n; ++i) {
        long long bin;
        if (span == 0) {
            bin = 0;
        } else {
            /* 极值相减可能溢出；等比例缩半后再定位箱位。 */
            double ratio = isfinite(span) ? (a[i] - lo) / span :
                (a[i] / 2.0 - lo / 2.0) / (hi / 2.0 - lo / 2.0);
            bin = (long long)floor(ratio * (double)bins);
            if (bin >= bins) bin = bins - 1;
            if (bin < 0) bin = 0;
        }
        counts[bin] += 1;
    }
    xrtFree(a);
    result = xrtValueArray();
    if (result == NULL) {
        xrtFree(counts);
        return xrtValueNull();
    }
    for (b = 0; b < bins; ++b) {
        if (!xrtValueArrayAppendNew(result, xrtValueInt(counts[b]))) {
            xrtValueRelease(result);
            xrtFree(counts);
            return xrtValueNull();
        }
    }
    xrtFree(counts);
    return result;
}
