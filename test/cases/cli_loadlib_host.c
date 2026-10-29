#include <stdlib.h>

__declspec(dllexport) long long cli_loadlib_value(void)
{
    return 42;
}

__declspec(dllexport) void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}
