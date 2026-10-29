#include <stdint.h>

#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_closure_capture_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_closure_present != 52 ) return 2;
    if ( xlir_nullable_closure_missing != 12 ) return 3;
    return 0;
}
