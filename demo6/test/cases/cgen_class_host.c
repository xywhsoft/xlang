#include <stdlib.h>
#include <string.h>

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        exit(1);
    }
}
