#include <stdbool.h>
#include <stdint.h>

#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_scalar_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_scalar_probe(true) != 7 ) return 2;
    if ( xlir_nullable_scalar_probe(false) != 13 ) return 3;
    if ( xlir_nullable_scalar_u64(true) != UINT64_MAX ) return 4;
    if ( xlir_nullable_scalar_u64(false) != 9ULL ) return 5;
    if ( xlir_nullable_scalar_float(true) != 3.5 ) return 6;
    if ( xlir_nullable_scalar_float(false) != 1.25 ) return 7;
    if ( !xlir_nullable_scalar_bool(true) ) return 8;
    if ( xlir_nullable_scalar_bool(false) ) return 9;
    {
        xlir_nullable_t4 missing = { false };
        xlir_nullable_t4 present = { true, 33 };
        xlir_nullable_t4 echoedMissing = xlir_nullable_scalar_echo(missing);
        xlir_nullable_t4 echoedPresent = xlir_nullable_scalar_echo(present);
        xlir_nullable_t4 checkedMissing = xlir_nullable_scalar_checked_echo(missing, true);
        xlir_nullable_t4 checkedPresent = xlir_nullable_scalar_checked_echo(present, true);
        xlir_nullable_t4 rejected = xlir_nullable_scalar_checked_echo(present, false);
        if ( echoedMissing.hasValue ) return 10;
        if ( !echoedPresent.hasValue || echoedPresent.value != 33 ) return 11;
        if ( checkedMissing.hasValue ) return 12;
        if ( !checkedPresent.hasValue || checkedPresent.value != 33 ) return 13;
        if ( rejected.hasValue ) return 14;
        xrtClearError();
    }
    return 0;
}
