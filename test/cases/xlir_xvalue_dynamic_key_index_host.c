#include <stdint.h>

#include "../../build/test/xlir_xvalue_dynamic_key_index_generated.c"

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) abort();
}

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_dynamic_key_object() != 42 ) return 2;
    if ( xlir_dynamic_key_safe_object() != 42 ) return 3;
    if ( xlir_dynamic_key_array() != 20 ) return 4;
    if ( xlir_dynamic_key_list() != 33 ) return 5;
    if ( xlir_dynamic_key_missing() != 71 ) return 6;
    if ( xlir_dynamic_key_invalid() != 73 ) return 7;
    return 0;
}
