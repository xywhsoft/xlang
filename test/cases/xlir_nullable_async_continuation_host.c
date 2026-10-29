#include <stdint.h>

#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_async_continuation_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_continuation_present != 38 ) return 2;
    if ( xlir_nullable_continuation_missing != 9 ) return 3;
    return 0;
}
