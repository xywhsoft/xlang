#include <stdint.h>

/* 仅测试包内部的分配长度计算，不构造巨大的真实数组。 */
#include "../statistics_native.c"

int main(void) {
    size_t bytes = 0;

    if (!xstat_double_bytes(3, &bytes) || bytes != 3 * sizeof(double))
        return 1;

    xrtClearError();
    bytes = 17;
    if (xstat_double_bytes(INT64_MAX, &bytes) || bytes != 17 ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_RANGE)
        return 2;

    xrtClearError();
    return 0;
}
