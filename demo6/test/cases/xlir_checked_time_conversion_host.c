#include <stdlib.h>
#include <string.h>

/* The quality gate generates this translation unit before compiling the host. */
#include "../../build/test/xlir_checked_time_conversion_generated.c"

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) abort();
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) abort();
}

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_test_invalid_time_string_recovered() != 73 ) return 2;
    if ( xlir_test_invalid_dynamic_time_recovered() != 91 ) return 3;
    return 0;
}
