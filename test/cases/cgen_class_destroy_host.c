#include <stdlib.h>
#include <stdio.h>

static long long gDestroyCount = 0;
static long long gDestroyValue = 0;

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "expect_i64 failed: actual=%lld expected=%lld\n", actual, expected);
        exit(1);
    }
}

void record_destroy(long long value)
{
    gDestroyCount += 1;
    gDestroyValue = value;
}

long long destroy_count(void)
{
    return gDestroyCount;
}

long long destroy_value(void)
{
    return gDestroyValue;
}
