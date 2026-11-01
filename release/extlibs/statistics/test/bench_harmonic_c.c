/* 非默认门禁的定向 CPU 基准；同一 XRT/编译器下分别链接旧、新 native 对照。 */
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../statistics_native.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

#ifdef __TINYC__
#include "../statistics_native.c"
#endif

static bool bench(size_t size, unsigned repeats, bool mixed)
{
    xvalue* input = xrtValueArray();
    if (input == NULL) return false;
    for (size_t i = 0; i < size; ++i) {
        double value = (double)(i % 16u + 1u);
        if (mixed && (i & 1u)) value = -value;
        if (!xrtValueArrayAppendNew(input, xrtValueFloat(value))) { xrtValueRelease(input); return false; }
    }
    clock_t begin = clock();
    double value = 0;
    for (unsigned i = 0; i < repeats; ++i) {
        xvalue* result = xstat_harmmean(input);
        bool ok = xrtGetError() == NULL && xrtValueGetFloat(result, &value) && isfinite(value);
        xrtValueRelease(result);
        if (!ok) { xrtValueRelease(input); return false; }
    }
    clock_t end = clock();
    xrtValueRelease(input);
    if (begin == (clock_t)-1 || end == (clock_t)-1) return false;
    printf("%s\t%zu\t%u\t%.3f\t%.17g\n", mixed ? "mixed" : "same", size, repeats,
        (double)(end - begin) * 1000.0 / CLOCKS_PER_SEC, value);
    return true;
}

int main(void)
{
    const size_t sizes[] = {32u, 1024u, 8192u};
    const unsigned repeats[] = {20000u, 1000u, 150u};
    for (size_t i = 0; i < 3; ++i) {
        if (!bench(sizes[i], repeats[i], false) || !bench(sizes[i], repeats[i], true)) return 1;
    }
    return 0;
}
