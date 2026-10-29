#include <stdbool.h>
#include <stdint.h>

#include "../../build/test/xlir_u64_dynamic_generated.c"

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_u64_dynamic_roundtrip() != UINT64_MAX ) return 2;
    if ( !xlir_u64_dynamic_text() ) return 3;
    if ( xlir_u64_dynamic_add() != 1ULL ) return 4;
    if ( xlir_u64_dynamic_sub() != UINT64_MAX ) return 5;
    if ( xlir_u64_dynamic_mul() != UINT64_MAX - 1ULL ) return 6;
    if ( xlir_u64_dynamic_from_text() != UINT64_MAX ) return 7;
    if ( xlir_u64_dynamic_from_i64(42) != 42ULL ) return 8;
    if ( xlir_u64_dynamic_from_f64(12.75) != 12ULL ) return 9;
    if ( !xlir_u64_dynamic_to_i64_rejected() ) return 10;
    if ( xlir_u64_dynamic_array() != UINT64_MAX ) return 11;
    if ( xlir_u64_dynamic_list() != UINT64_MAX ) return 12;
    if ( xlir_u64_dynamic_dict() != UINT64_MAX ) return 13;
    if ( xlir_u64_dynamic_class() != UINT64_MAX ) return 14;
    if ( !xlir_u64_dynamic_negative_rejected() ) return 15;
    if ( xlir_u64_dynamic_xcall() != UINT64_MAX ) return 16;
    if ( xlir_u64_dynamic_task_all != UINT64_MAX ) return 17;
    return 0;
}
