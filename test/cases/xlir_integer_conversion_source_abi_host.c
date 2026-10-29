#include <stdbool.h>
#include <stdint.h>

#ifdef XLIR_USE_LEGACY_GENERATED
#include "../../build/test/xlir_integer_conversion_source_abi_legacy.c"
#else
#include "../../build/test/xlir_integer_conversion_source_abi_generated.c"
#endif

int main(void)
{
#ifndef XLIR_USE_LEGACY_GENERATED
    if ( !xlir_module_init() ) return 1;
#else
    if ( !xl6_temp_managed_runtime_init() ) return 1;
#endif
    if ( xlir_integer_from_string() != 42 ) return 2;
    if ( xlir_u64_from_string() != UINT64_MAX ) return 3;
    if ( xlir_integer_from_u64() != INT64_MAX ) return 4;
    if ( xlir_integer_from_float() != 19 ) return 5;
    if ( xlir_integer_from_xvalue_string() != 91 ) return 6;
    if ( xlir_u64_from_xvalue_string() != UINT64_MAX ) return 7;
    if ( !xlir_integer_invalid_string() ) return 8;
    if ( !xlir_u64_negative_string() ) return 9;
    if ( !xlir_u64_invalid_string() ) return 10;
    if ( !xlir_integer_u64_wrap() ) return 11;
    return 0;
}
