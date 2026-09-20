#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int64_t native_add_i64(int64_t left, int64_t right)
{
    return left + right;
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "legacy xcall mismatch: %lld != %lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}
