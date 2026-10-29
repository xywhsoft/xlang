#include <stdlib.h>

/* The quality gate generates this translation unit before compiling the host. */
#include "../../build/test/xlir_bool_conversion_abi_generated.c"

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) abort();
}

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_bool_dynamic_text_truth() != 1 ) return 2;
    return 0;
}
