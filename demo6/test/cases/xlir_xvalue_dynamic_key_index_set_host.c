#include <stdint.h>

#include "../../build/test/xlir_xvalue_dynamic_key_index_set_generated.c"

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) abort();
}

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_dynamic_key_set_object() != 81 ) return 2;
    if ( xlir_dynamic_key_set_array() != 82 ) return 3;
    if ( xlir_dynamic_key_set_list() != 83 ) return 4;
    return 0;
}
