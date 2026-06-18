#include <stdlib.h>

const char* native_text(void)
{
    return "native text";
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}
