#include <stdint.h>

#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_async_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_async_present != 27 ) return 2;
    if ( xlir_nullable_async_missing != 7 ) return 3;
    return 0;
}
