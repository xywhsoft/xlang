#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "default class layout mismatch: %lld != %lld\n", actual, expected);
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "default class string mismatch: %s != %s\n",
                actual != NULL ? actual : "(null)",
                expected != NULL ? expected : "(null)");
        exit(1);
    }
}
