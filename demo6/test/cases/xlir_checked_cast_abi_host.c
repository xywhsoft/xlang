#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The quality gate injects the generated XLIR translation unit with
 * `-include`, matching the internal module-initializer embedding ABI. */
static int xlir_module_init(void);
static int xlir_checked_cast_point_token;

void* xlir_checked_cast_point_source(void)
{
    return &xlir_checked_cast_point_token;
}

void xlir_checked_cast_expect_point(void* actual, void* expected)
{
    if ( actual != expected ) exit(1);
}

void xlir_checked_cast_expect_c_bool(bool actual, bool expected)
{
    if ( actual != expected ) exit(1);
}

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "checked cast mismatch: %lld != %lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "checked cast string mismatch: %s != %s\n",
                actual != NULL ? actual : "(null)",
                expected != NULL ? expected : "(null)");
        exit(1);
    }
}

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
