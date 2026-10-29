#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_sentinel_coalesce_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_sentinel_var(1) != 8 ) return 2;
    if ( xlir_nullable_sentinel_var(0) != 117 ) return 3;
    if ( xlir_nullable_sentinel_class(1) != 9 ) return 4;
    if ( xlir_nullable_sentinel_class(0) != 141 ) return 5;
    if ( xlir_nullable_literal_coalesce() != 23 ) return 6;
    return 0;
}
