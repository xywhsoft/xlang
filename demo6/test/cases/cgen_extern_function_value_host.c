#include <stdlib.h>

long long native_add_i64(long long left, long long right)
{
    return left + right;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}
