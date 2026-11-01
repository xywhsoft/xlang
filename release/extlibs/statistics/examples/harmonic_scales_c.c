/* 与 XLang 使用相同输入/断言；本示例只依赖包的公开 C API。 */
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../statistics_native.h"
#include <math.h>
#include <stdio.h>

#ifdef __TINYC__
#include "../statistics_native.c"
#endif

static bool mean(const double* a, size_t n, double expected)
{
    xvalue* input = xrtValueArray();
    bool ok = input != NULL;
    for (size_t i = 0; ok && i < n; ++i) ok = xrtValueArrayAppendNew(input, xrtValueFloat(a[i]));
    xvalue* result = ok ? xstat_harmmean(input) : NULL;
    double actual;
    if (isfinite(expected)) {
        ok = ok && xrtGetError() == NULL && xrtValueGetFloat(result, &actual) && fabs(actual - expected) < 1e-12;
    } else ok = ok && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    for (size_t i = 0; ok && i < n; ++i) ok = xrtValueGetFloat(xrtValueArrayGet(input, i), &actual) && actual == a[i];
    xrtValueRelease(result); xrtValueRelease(input); xrtClearError();
    return ok;
}

int main(void)
{
    const double signed_values[] = {1e-300, -1e-300, 1, 2};
    const double scales[] = {1e-300, -1e-300, 1e-100, -1e-100, 1, 2};
    const double negative[] = {1e-300, -1e-300, -1, -2};
    const double positive[] = {1, 2, 4}, zero[] = {1, -1};
    if (!mean(signed_values, 4, 8.0 / 3.0)) return 1;
    puts("signed mean: 8/3");
    if (!mean(scales, 6, 4)) return 1;
    puts("several scales: 4");
    if (!mean(negative, 4, -8.0 / 3.0)) return 1;
    puts("negative tail: -8/3");
    if (!mean(positive, 3, 12.0 / 7.0)) return 1;
    puts("same-sign mean: 12/7");
    if (!mean(zero, 2, NAN)) return 1;
    puts("undefined: rejected");
    return 0;
}
