#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#include "../statistics_native.h"
#include <inttypes.h>
#include <stdio.h>

static int print_number(const char *name, xvalue *value)
{
    double number;
    if (!value || !xrtValueGetFloat(value, &number)) {
        xrtValueRelease(value);
        return 0;
    }
    printf("%s: %.1f\n", name, number);
    xrtValueRelease(value);
    return 1;
}

int main(void)
{
    const int values[] = {2, 4, 4, 4, 5, 5, 7, 9};
    xvalue *data = xrtValueArray();
    size_t i;
    int ok;
    if (!data) return 1;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (!xrtValueArrayAppendNew(data, xrtValueInt(values[i]))) {
            xrtValueRelease(data);
            return 1;
        }
    }

    printf("count: %" PRId64 "\n", xstat_count(data));
    ok = print_number("sum", xstat_sum(data)) &&
        print_number("mean", xstat_mean(data)) &&
        print_number("median", xstat_median(data)) &&
        print_number("min", xstat_min(data)) &&
        print_number("max", xstat_max(data)) &&
        print_number("range", xstat_range(data)) &&
        print_number("stdev", xstat_stdev(data));
    xrtValueRelease(data);
    return ok ? 0 : 1;
}

/* 示例自包含：xl run 只接收当前 C 文件，统计实现随此编译单元进入。 */
#include "../statistics_native.c"
