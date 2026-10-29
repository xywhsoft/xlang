#include <stdint.h>

#include "../../build/test/xlir_typed_dict_scalar_set_generated.c"

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) abort();
}

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_typed_dict_literal_set() != 81 ) return 2;
    if ( xlir_typed_dict_string_set() != 82 ) return 3;
    return 0;
}
