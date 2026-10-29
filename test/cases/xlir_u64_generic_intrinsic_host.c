#include <stdint.h>

#include "../../build/test/xlir_u64_generic_intrinsic_generated.c"

int main(void)
{
    if ( xlir_module_init() != 1 ) return 1;
    if ( xlir_u64_generic_literal != 2026ULL ) return 2;
    if ( xlir_u64_generic_bool != 1ULL ) return 3;
    if ( xlir_u64_generic_identity != UINT64_MAX ) return 4;
    return xlir_u64_generic_text ? 0 : 5;
}
