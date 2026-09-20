#include <stdint.h>

uint64_t xlir_u64_external = 0xfedcba9876543210ULL;

#include "../../build/test/xlir_u64_scalar_generated.c"

int main(void)
{
    uint64_t failed;
    if ( xlir_u64_maximum != UINT64_MAX || xlir_u64_zero != 0ULL ) return 1;
    if ( xlir_u64_high_bit() != 9223372036854775808ULL ) return 2;
    if ( xlir_u64_increment(9223372036854775807ULL) != 9223372036854775808ULL ) return 3;
    if ( xlir_u64_half(UINT64_MAX) != 9223372036854775807ULL ) return 4;
    if ( !xlir_u64_is_high(9223372036854775808ULL) ||
         xlir_u64_is_high(9223372036854775807ULL) ) return 5;
    if ( xlir_u64_default() != 0ULL ) return 6;
    if ( xlir_u64_mask(0xf000000000000000ULL) != 15ULL ) return 7;
    if ( xlir_u64_not(0ULL) != UINT64_MAX ) return 8;
    if ( xlir_u64_literal_convert() != 2026ULL ) return 9;
    if ( xlir_u64_bool_convert(true) != 1ULL || xlir_u64_bool_convert(false) != 0ULL ) return 10;
    if ( xlir_u64_read_external() != xlir_u64_external ) return 11;
    if ( !xlir_u64_text_ok(UINT64_MAX) ) return 12;
    failed = xlir_u64_fail();
    return failed == 0ULL && xrtGetError() != NULL ? 0 : 13;
}
