#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../statistics_native.h"
#include <math.h>
#include <float.h>
#include <stdio.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "statistics test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int append_numbers(xvalue *array, const double *numbers, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        if (!xrtValueArrayAppendNew(array, xrtValueFloat(numbers[i]))) return 0;
    }
    return 1;
}

static int equal_result(xvalue *value, double expected)
{
    double actual;
    int ok = value && xrtValueType(value) == XVALUE_FLOAT &&
        xrtValueGetFloat(value, &actual) && fabs(actual - expected) < 1e-10;
    xrtValueRelease(value);
    return ok;
}

/* 以测试侧整数模型核对同一批数据的排序、离散度和分箱。 */
static int test_integer_reference(void)
{
    uint32_t seed = 0x7f4a91c3u;
    unsigned trial;
    for (trial = 0; trial < 512u; ++trial) {
        int sorted[32], histogram[7] = {0};
        int64_t sum = 0, squares = 0;
        size_t n = 1u + trial % 32u, i, j;
        int mode = 0, best = 0;
        xvalue *values = xrtValueArray();
        xvalue *buckets;
        CHECK(values != NULL);
        for (i = 0; i < n; ++i) {
            int number;
            seed = seed * 1664525u + 1013904223u;
            number = (int)((seed >> 8u) % 201u) - 100;
            sorted[i] = number;
            sum += number;
            squares += (int64_t)number * number;
            CHECK(xrtValueArrayAppendNew(values, xrtValueInt(number)));
        }
        for (i = 1; i < n; ++i) {
            int item = sorted[i];
            j = i;
            while (j > 0 && sorted[j - 1] > item) {
                sorted[j] = sorted[j - 1];
                --j;
            }
            sorted[j] = item;
        }
        for (i = 0; i < n; ++i) {
            int frequency = 0;
            for (j = 0; j < n; ++j)
                if (sorted[j] == sorted[i]) ++frequency;
            if (frequency > best) { best = frequency; mode = sorted[i]; }
        }
        CHECK(xstat_count(values) == (int64_t)n);
        CHECK(equal_result(xstat_sum(values), (double)sum));
        CHECK(equal_result(xstat_mean(values), (double)sum / (double)n));
        CHECK(equal_result(xstat_median(values),
            ((double)sorted[(n - 1u) / 2u] + (double)sorted[n / 2u]) / 2.0));
        CHECK(equal_result(xstat_mode(values), (double)mode));
        CHECK(equal_result(xstat_min(values), (double)sorted[0]));
        CHECK(equal_result(xstat_max(values), (double)sorted[n - 1u]));
        CHECK(equal_result(xstat_range(values),
            (double)(sorted[n - 1u] - sorted[0])));
        CHECK(equal_result(xstat_variance(values),
            (double)((int64_t)n * squares - sum * sum) / ((double)n * (double)n)));
        if (n > 1u)
            CHECK(equal_result(xstat_variance_sample(values),
                (double)((int64_t)n * squares - sum * sum) /
                ((double)n * (double)(n - 1u))));
        for (i = 0; i < 2u; ++i) {
            size_t rank = (n - 1u) * (i == 0u ? 1u : 3u);
            size_t lower = rank / 4u, upper = lower + (rank % 4u != 0u);
            double expected = (double)sorted[lower] +
                (double)(sorted[upper] - sorted[lower]) * (double)(rank % 4u) / 4.0;
            CHECK(equal_result(xstat_percentile(values, i == 0u ? 25.0 : 75.0),
                expected));
        }
        for (i = 0; i < n; ++i) {
            int bin = sorted[0] == sorted[n - 1u] ? 0 :
                (sorted[i] - sorted[0]) * 7 /
                (sorted[n - 1u] - sorted[0]);
            if (bin == 7) bin = 6;
            histogram[bin]++;
        }
        buckets = xstat_histogram(values, 7);
        CHECK(buckets != NULL && xrtValueCount(buckets) == 7);
        for (i = 0; i < 7u; ++i) {
            int64_t actual = -1;
            CHECK(xrtValueGetInt(xrtValueArrayAt(buckets, (int64_t)i), &actual));
            CHECK(actual == histogram[i]);
        }
        xrtValueRelease(buckets);
        xrtValueRelease(values);
    }
    return 0;
}

int main(void)
{
    const double numbers[] = {2, 4, 4, 4, 5, 5, 7, 9};
    const double sample_x[] = {1, 2, 3};
    const double sample_y[] = {2, 4, 6};
    xvalue *values = xrtValueArray();
    xvalue *empty = xrtValueArray();
    xvalue *paired_x = xrtValueArray();
    xvalue *paired_y = xrtValueArray();
    xvalue *invalid;
    xvalue *extreme = xrtValueArray();
    xvalue *mean_order = xrtValueArray();
    xvalue *buckets;
    xvalue *slot;
    int64 count;

    CHECK(test_integer_reference() == 0);

    CHECK(values && empty && append_numbers(values, numbers, 8));
    CHECK(xstat_count(values) == 8);
    CHECK(equal_result(xstat_sum(values), 40));
    CHECK(equal_result(xstat_mean(values), 5));
    CHECK(equal_result(xstat_median(values), 4.5));
    CHECK(equal_result(xstat_mode(values), 4));
    {
        const double tied[] = {7, 5, 7, 5};
        const double unique[] = {7, 5, 3};
        xvalue *tie_values = xrtValueArray();
        xvalue *unique_values = xrtValueArray();
        CHECK(tie_values && unique_values &&
            append_numbers(tie_values, tied, 4) &&
            append_numbers(unique_values, unique, 3));
        CHECK(equal_result(xstat_mode(tie_values), 5));
        CHECK(equal_result(xstat_mode(unique_values), 3));
        xrtValueRelease(tie_values);
        xrtValueRelease(unique_values);
    }
    CHECK(equal_result(xstat_min(values), 2));
    CHECK(equal_result(xstat_max(values), 9));
    CHECK(equal_result(xstat_range(values), 7));
    {
        xvalue *mixed = xrtValueArray();
        xvalue *failed;
        CHECK(mixed && xrtValueArrayAppendNew(mixed, xrtValueFloat(2)) &&
            xrtValueArrayAppendNew(mixed,
                xrtValueString((xstrview){ "bad", 3u })));
        failed = xstat_min(mixed);
        CHECK(failed && xrtValueType(failed) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(failed);
        xrtValueRelease(mixed);
        CHECK(equal_result(xstat_range(values), 7) && xrtGetError() == NULL);
    }
    CHECK(equal_result(xstat_variance(values), 4));
    CHECK(equal_result(xstat_stdev(values), 2));
    CHECK(equal_result(xstat_percentile(values, 50), 4.5));
    {
        xvalue *failed;
        CHECK(xrtMemDebugFailAfter(0));
        failed = xstat_min(values);
        CHECK(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        CHECK(failed && xrtValueType(failed) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtValueRelease(failed);
        CHECK(equal_result(xstat_max(values), 9) && xrtGetError() == NULL);
    }
    {
        xvalue *failed = xstat_mean(empty);
        CHECK(failed && xrtValueType(failed) == XVALUE_NULL && xrtGetError());
        xrtValueRelease(failed);
        CHECK(equal_result(xstat_sum(values), 40));
        CHECK(xrtGetError() == NULL);

        failed = xstat_mean(empty);
        CHECK(failed && xrtValueType(failed) == XVALUE_NULL && xrtGetError());
        xrtValueRelease(failed);
        CHECK(xstat_count(values) == 8 && xrtGetError() == NULL);
    }
    {
        xvalue *failed;
        CHECK(xrtMemDebugFailAfter(0));
        failed = xstat_sum(values);
        CHECK(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        CHECK(failed && xrtValueType(failed) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtValueRelease(failed);
        xrtClearError();
        CHECK(equal_result(xstat_sum(values), 40));
    }
    {
        size_t fail_at;
        for (fail_at = 0; fail_at < 8; ++fail_at) {
            xvalue *result;
            int triggered;
            CHECK(xrtMemDebugFailAfter(fail_at));
            result = xstat_quartiles(values);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(result && xrtValueType(result) == XVALUE_NULL &&
                    xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            } else {
                CHECK(result && xrtValueType(result) == XVALUE_ARRAY &&
                    xrtGetError() == NULL);
            }
            xrtValueRelease(result);
            xrtClearError();
        }
        CHECK(xstat_count(values) == 8);
    }
    {
        const double cancellation[] = {1e16, 1, -1e16};
        xvalue *ordered = xrtValueArray();
        xvalue *result;
        double actual;
        CHECK(ordered && append_numbers(ordered, cancellation, 3));
        result = xstat_sum(ordered);
        CHECK(result && xrtValueGetFloat(result, &actual) && actual == 1);
        xrtValueRelease(result);
        result = xstat_mean(ordered);
        CHECK(result && xrtValueGetFloat(result, &actual) &&
            fabs(actual - 1.0 / 3.0) < 1e-15);
        xrtValueRelease(result);
        xrtValueRelease(ordered);
    }
    {
        const double subnormal = DBL_MIN * DBL_EPSILON;
        const double residuals[][3] = {
            {DBL_MAX, 1.0, -DBL_MAX},
            {DBL_MAX, subnormal * 2.0, -DBL_MAX},
            {DBL_MAX, -DBL_MAX, subnormal * 2.0}
        };
        const double expected[] = {1.0, subnormal * 2.0, subnormal * 2.0};
        size_t i;
        for (i = 0; i < 3; ++i) {
            xvalue *ordered = xrtValueArray();
            xvalue *result;
            double actual;
            CHECK(ordered && append_numbers(ordered, residuals[i], 3));
            result = xstat_sum(ordered);
            CHECK(result && xrtValueGetFloat(result, &actual) && actual == expected[i]);
            xrtValueRelease(result);
            xrtValueRelease(ordered);
        }
    }
    {
        const double subnormal = DBL_MIN * DBL_EPSILON;
        const double residuals[][3] = {
            {DBL_MAX, subnormal * 3.0, -DBL_MAX},
            {DBL_MAX, -DBL_MAX, subnormal * 3.0}
        };
        size_t i;
        for (i = 0; i < 2; ++i) {
            xvalue *ordered = xrtValueArray();
            xvalue *result;
            double actual;
            CHECK(ordered && append_numbers(ordered, residuals[i], 3));
            result = xstat_mean(ordered);
            CHECK(result && xrtValueGetFloat(result, &actual) && actual == subnormal);
            xrtValueRelease(result);
            xrtValueRelease(ordered);
        }
    }
    {
        const double subnormal = DBL_MIN * DBL_EPSILON;
        const double smallest[] = {subnormal, subnormal};
        xvalue *tiny = xrtValueArray();
        xvalue *result;
        double actual;
        CHECK(subnormal > 0);
        CHECK(tiny && append_numbers(tiny, smallest, 2));
        result = xstat_sum(tiny);
        CHECK(result && xrtValueGetFloat(result, &actual) &&
            actual == subnormal * 2);
        xrtValueRelease(result);
        result = xstat_mean(tiny);
        CHECK(result && xrtValueGetFloat(result, &actual) &&
            actual == subnormal);
        xrtValueRelease(result);
        result = xstat_percentile(tiny, 50);
        CHECK(result && xrtValueGetFloat(result, &actual) &&
            actual == subnormal);
        xrtValueRelease(result);
        xrtValueRelease(tiny);

        /* 逐项除以样本数会将两个最小次正规偏差都舍入成零。 */
        tiny = xrtValueArray();
        CHECK(tiny && xrtValueArrayAppendNew(tiny, xrtValueFloat(0)));
        CHECK(xrtValueArrayAppendNew(tiny, xrtValueFloat(subnormal * 2)));
        result = xstat_mad(tiny);
        CHECK(result && xrtValueGetFloat(result, &actual) &&
            actual == subnormal);
        xrtValueRelease(result);
        xrtValueRelease(tiny);

        tiny = xrtValueArray();
        CHECK(tiny && xrtValueArrayAppendNew(tiny,
            xrtValueFloat(subnormal)));
        CHECK(xrtValueArrayAppendNew(tiny,
            xrtValueFloat(subnormal * 2)));
        result = xstat_percentile(tiny, 50);
        CHECK(result && xrtValueGetFloat(result, &actual) &&
            actual == subnormal * 2);
        xrtValueRelease(result);
        xrtValueRelease(tiny);
    }
    {
        xvalue *large = xrtValueArray();
        xvalue *mean;
        double actual = 0;
        CHECK(large && xrtValueArrayAppendNew(large, xrtValueFloat(DBL_MAX)));
        CHECK(xrtValueArrayAppendNew(large, xrtValueFloat(DBL_MAX)));
        mean = xstat_harmmean(large);
        CHECK(mean && xrtValueGetFloat(mean, &actual));
        CHECK(isfinite(actual) && fabs(actual / DBL_MAX - 1.0) < 1e-12);
        xrtValueRelease(mean);
        xrtValueRelease(large);
    }
    {
        xvalue *tiny = xrtValueArray();
        xvalue *mean;
        double actual = 0;
        CHECK(tiny && xrtValueArrayAppendNew(tiny, xrtValueFloat(1e-320)));
        CHECK(xrtValueArrayAppendNew(tiny, xrtValueFloat(1e-320)));
        mean = xstat_harmmean(tiny);
        CHECK(mean && xrtValueGetFloat(mean, &actual));
        CHECK(actual > 0 && fabs(actual / 1e-320 - 1.0) < 1e-12);
        xrtValueRelease(mean);
        xrtValueRelease(tiny);
    }
    {
        const double smallest = DBL_MIN * DBL_EPSILON;
        xvalue *tiny = xrtValueArray();
        xvalue *mean;
        double actual = 0;
        CHECK(tiny && xrtValueArrayAppendNew(tiny, xrtValueFloat(smallest)));
        CHECK(xrtValueArrayAppendNew(tiny, xrtValueFloat(smallest)));
        mean = xstat_geomean(tiny);
        CHECK(mean && xrtValueGetFloat(mean, &actual) && actual == smallest);
        xrtValueRelease(mean);
        xrtValueRelease(tiny);
    }
    {
        xvalue *pair = xrtValueArray();
        xvalue *mean;
        double actual = 0;
        CHECK(pair && xrtValueArrayAppendNew(pair, xrtValueFloat(2)));
        CHECK(xrtValueArrayAppendNew(pair, xrtValueFloat(8)));
        CHECK(equal_result(xstat_geomean(pair), 4));
        xrtValueRelease(pair);

        pair = xrtValueArray();
        CHECK(pair && xrtValueArrayAppendNew(pair, xrtValueFloat(DBL_MAX)));
        CHECK(xrtValueArrayAppendNew(pair, xrtValueFloat(DBL_MAX)));
        mean = xstat_geomean(pair);
        CHECK(mean && xrtValueGetFloat(mean, &actual) && isfinite(actual) &&
            fabs(actual / DBL_MAX - 1.0) < 1e-12);
        xrtValueRelease(mean);
        xrtValueRelease(pair);

        pair = xrtValueArray();
        CHECK(pair != NULL);
        for (size_t i = 0; i < 100; ++i)
            CHECK(xrtValueArrayAppendNew(pair, xrtValueFloat(DBL_MAX)));
        mean = xstat_geomean(pair);
        CHECK(mean && xrtValueGetFloat(mean, &actual) && isfinite(actual) &&
            actual <= DBL_MAX && fabs(actual / DBL_MAX - 1.0) < 1e-12);
        xrtValueRelease(mean);
        xrtValueRelease(pair);

        pair = xrtValueArray();
        CHECK(pair && xrtValueArrayAppendNew(pair, xrtValueFloat(0)));
        CHECK(xrtValueArrayAppendNew(pair, xrtValueFloat(1)));
        mean = xstat_geomean(pair);
        CHECK(mean && xrtValueType(mean) == XVALUE_NULL && xrtGetError() &&
            xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(mean);
        xrtValueRelease(pair);
    }
    {
        xvalue *cancel = xrtValueArray();
        xvalue *mean;
        double actual = 0;
        CHECK(cancel && xrtValueArrayAppendNew(cancel, xrtValueFloat(1e-16)));
        CHECK(xrtValueArrayAppendNew(cancel, xrtValueFloat(1.0)));
        CHECK(xrtValueArrayAppendNew(cancel, xrtValueFloat(-1e-16)));
        mean = xstat_harmmean(cancel);
        CHECK(mean && xrtValueGetFloat(mean, &actual));
        CHECK(fabs(actual - 3.0) < 1e-9);
        xrtValueRelease(mean);
        xrtValueRelease(cancel);
    }
    CHECK(paired_x && paired_y);
    CHECK(append_numbers(paired_x, sample_x, 3));
    CHECK(append_numbers(paired_y, sample_y, 3));
    {
        xvalue *failed = xstat_covariance(empty, paired_y);
        CHECK(failed && xrtValueType(failed) == XVALUE_NULL && xrtGetError());
        xrtValueRelease(failed);
        CHECK(equal_result(xstat_covariance(paired_x, paired_y), 4.0 / 3.0));
        CHECK(xrtGetError() == NULL);
    }
    {
        size_t fail_at;
        for (fail_at = 0; fail_at < 2; ++fail_at) {
            xvalue *failed;
            CHECK(xrtMemDebugFailAfter(fail_at));
            failed = xstat_covariance(paired_x, paired_y);
            CHECK(xrtMemDebugFailTriggered());
            xrtMemDebugFailClear();
            CHECK(failed && xrtValueType(failed) == XVALUE_NULL &&
                xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            xrtValueRelease(failed);
            xrtClearError();
        }
    }
    CHECK(equal_result(xstat_covariance(paired_x, paired_y), 4.0 / 3.0));
    CHECK(equal_result(xstat_covariance_sample(paired_x, paired_y), 2));
    CHECK(equal_result(xstat_correlation(paired_x, paired_y), 1));
    {
        xvalue *fit = xstat_linear_fit(paired_x, paired_y);
        double slope = 0;
        double intercept = 0;
        CHECK(fit && xrtValueType(fit) == XVALUE_ARRAY);
        CHECK(xrtValueGetFloat(xrtValueArrayAt(fit, 0), &slope));
        CHECK(xrtValueGetFloat(xrtValueArrayAt(fit, 1), &intercept));
        CHECK(fabs(slope - 2) < 1e-12 && fabs(intercept) < 1e-12);
        xrtValueRelease(fit);
    }

    /* 失败不应返回伪正常值；每个负例后清除预期错误。 */
    invalid = xstat_percentile(values, -1);
    CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL);
    xrtValueRelease(invalid);
    xrtClearError();
    invalid = xstat_percentile(values, NAN);
    CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL);
    xrtValueRelease(invalid);
    xrtClearError();
    invalid = xstat_histogram(values, 100001);
    CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL);
    xrtValueRelease(invalid);
    xrtClearError();
    CHECK(xrtMemDebugFailAfter(1));
    invalid = xstat_histogram(values, 0);
    CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL &&
        xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE &&
        !xrtMemDebugFailTriggered());
    xrtValueRelease(invalid);
    xrtMemDebugFailClear();
    xrtClearError();
    invalid = xstat_mean(empty);
    CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL);
    xrtValueRelease(invalid);
    xrtClearError();
    CHECK(xstat_count(xrtValueNull()) == 0);
    {
        const double cancelling[] = {1, -1};
        xvalue *pair = xrtValueArray();
        CHECK(pair && append_numbers(pair, cancelling, 2));
        invalid = xstat_harmmean(pair);
        CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL && xrtGetError());
        xrtValueRelease(invalid);
        xrtValueRelease(pair);
        xrtClearError();
    }

    CHECK(mean_order);
    CHECK(xrtValueArrayAppendNew(mean_order, xrtValueFloat(DBL_MAX)));
    CHECK(xrtValueArrayAppendNew(mean_order, xrtValueFloat(DBL_MAX)));
    CHECK(xrtValueArrayAppendNew(mean_order, xrtValueFloat(-DBL_MAX)));
    {
        xvalue *average = xstat_mean(mean_order);
        double actual = 0;
        CHECK(average && xrtValueGetFloat(average, &actual));
        CHECK(isfinite(actual) && fabs(actual / DBL_MAX - 1.0 / 3.0) < 1e-12);
        xrtValueRelease(average);
        average = xstat_sum(mean_order);
        CHECK(average && xrtValueGetFloat(average, &actual));
        CHECK(isfinite(actual) && fabs(actual / DBL_MAX - 1.0) < 1e-12);
        xrtValueRelease(average);
        average = xstat_stdev(mean_order);
        CHECK(average && xrtValueGetFloat(average, &actual));
        CHECK(isfinite(actual) &&
            fabs(actual / DBL_MAX - sqrt(8.0 / 9.0)) < 1e-12);
        xrtValueRelease(average);
    }
    xrtValueRelease(mean_order);

    {
        xvalue *constant = xrtValueArray();
        xvalue *average;
        double actual = 0;
        CHECK(constant);
        CHECK(xrtValueArrayAppendNew(constant, xrtValueFloat(DBL_MAX)));
        CHECK(xrtValueArrayAppendNew(constant, xrtValueFloat(DBL_MAX)));
        CHECK(xrtValueArrayAppendNew(constant, xrtValueFloat(DBL_MAX)));
        average = xstat_mean(constant);
        CHECK(average && xrtValueGetFloat(average, &actual) && actual == DBL_MAX);
        xrtValueRelease(average);
        average = xstat_variance(constant);
        CHECK(average && xrtValueGetFloat(average, &actual) && actual == 0);
        xrtValueRelease(average);
        average = xstat_stdev(constant);
        CHECK(average && xrtValueGetFloat(average, &actual) && actual == 0);
        xrtValueRelease(average);
        average = xstat_mad(constant);
        CHECK(average && xrtValueGetFloat(average, &actual) && actual == 0);
        xrtValueRelease(average);
        xrtValueRelease(constant);
    }

    CHECK(extreme);
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(-DBL_MAX)));
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(DBL_MAX)));
    CHECK(equal_result(xstat_mean(extreme), 0));
    CHECK(equal_result(xstat_percentile(extreme, 50), 0));
    CHECK(equal_result(xstat_correlation(extreme, extreme), 1));
    {
        xvalue *fit = xstat_linear_fit(extreme, extreme);
        double slope = 0;
        double intercept = 0;
        CHECK(fit && xrtValueType(fit) == XVALUE_ARRAY);
        CHECK(xrtValueGetFloat(xrtValueArrayAt(fit, 0), &slope));
        CHECK(xrtValueGetFloat(xrtValueArrayAt(fit, 1), &intercept));
        CHECK(fabs(slope - 1) < 1e-12 && intercept == 0);
        xrtValueRelease(fit);
    }
    {
        const double shifted_x[] = {9e307, 1.1e308};
        const double shifted_y[] = {8e307, 1.2e308};
        xvalue *x = xrtValueArray();
        xvalue *y = xrtValueArray();
        xvalue *fit;
        double slope = 0;
        double intercept = 0;
        CHECK(x && y && append_numbers(x, shifted_x, 2) &&
            append_numbers(y, shifted_y, 2));
        fit = xstat_linear_fit(x, y);
        CHECK(fit && xrtValueType(fit) == XVALUE_ARRAY);
        CHECK(xrtValueGetFloat(xrtValueArrayAt(fit, 0), &slope));
        CHECK(xrtValueGetFloat(xrtValueArrayAt(fit, 1), &intercept));
        CHECK(fabs(slope - 2) < 1e-12 && isfinite(intercept) &&
            fabs(intercept / 1e308 + 1) < 1e-12);
        xrtValueRelease(fit);
        xrtValueRelease(x);
        xrtValueRelease(y);
    }
    {
        const double narrow_x[] = {0, 1e-300};
        const double wide_y[] = {0, 1e308};
        xvalue *x = xrtValueArray();
        xvalue *y = xrtValueArray();
        CHECK(x && y && append_numbers(x, narrow_x, 2) &&
            append_numbers(y, wide_y, 2));
        invalid = xstat_linear_fit(x, y);
        CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(invalid);
        xrtClearError();
        xrtValueRelease(x);
        xrtValueRelease(y);
    }
    invalid = xstat_range(extreme);
    CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(invalid);
    xrtClearError();
    invalid = xstat_variance(extreme);
    CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(invalid);
    xrtClearError();
    {
        xvalue *deviation = xstat_mad(extreme);
        double actual = 0;
        CHECK(deviation && xrtValueGetFloat(deviation, &actual));
        CHECK(isfinite(actual) && fabs(actual / DBL_MAX - 1.0) < 1e-12);
        xrtValueRelease(deviation);
    }
    {
        xvalue *deviation = xstat_stdev(extreme);
        double actual = 0;
        CHECK(deviation && xrtValueGetFloat(deviation, &actual));
        CHECK(isfinite(actual) && fabs(actual / DBL_MAX - 1.0) < 1e-12);
        xrtValueRelease(deviation);
    }
    buckets = xstat_histogram(extreme, 2);
    CHECK(buckets && xrtValueType(buckets) == XVALUE_ARRAY);
    slot = xrtValueArrayAt(buckets, 0);
    CHECK(slot && xrtValueGetInt(slot, &count) && count == 1);
    slot = xrtValueArrayAt(buckets, 1);
    CHECK(slot && xrtValueGetInt(slot, &count) && count == 1);
    xrtValueRelease(buckets);
    xrtValueRelease(extreme);
    extreme = xrtValueArray();
    CHECK(extreme);
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(-1e308)));
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(1e308)));
    {
        xvalue *deviation = xstat_stdev_sample(extreme);
        double actual = 0;
        CHECK(deviation && xrtValueGetFloat(deviation, &actual));
        CHECK(isfinite(actual) && fabs(actual / 1e308 - sqrt(2.0)) < 1e-12);
        xrtValueRelease(deviation);
    }
    xrtValueRelease(extreme);
    extreme = xrtValueArray();
    CHECK(extreme);
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(-9e153)));
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(9e153)));
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(-9e153)));
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(9e153)));
    {
        xvalue *variance = xstat_variance(extreme);
        double actual = 0;
        CHECK(variance && xrtValueGetFloat(variance, &actual));
        CHECK(isfinite(actual) && fabs(actual / 8.1e307 - 1.0) < 1e-12);
        xrtValueRelease(variance);
        variance = xstat_variance_sample(extreme);
        CHECK(variance && xrtValueGetFloat(variance, &actual));
        CHECK(isfinite(actual) && fabs(actual / 1.08e308 - 1.0) < 1e-12);
        xrtValueRelease(variance);
    }
    xrtValueRelease(extreme);
    extreme = xrtValueArray();
    CHECK(extreme);
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(DBL_MAX)));
    CHECK(xrtValueArrayAppendNew(extreme, xrtValueFloat(DBL_MAX)));
    invalid = xstat_sum(extreme);
    CHECK(invalid && xrtValueType(invalid) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(invalid);
    xrtClearError();
    xrtValueRelease(extreme);

    xrtValueRelease(values);
    xrtValueRelease(empty);
    xrtValueRelease(paired_x);
    xrtValueRelease(paired_y);
    return 0;
}
