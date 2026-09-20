#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_sentinel_assign_if_null_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_sentinel_var_assign(1) != 8 ) return 2;
    if ( xlir_nullable_sentinel_var_assign(0) != 117 ) return 3;
    if ( xlir_nullable_sentinel_class_assign(1) != 9 ) return 4;
    if ( xlir_nullable_sentinel_class_assign(0) != 141 ) return 5;
    if ( xlir_nonnull_assign_if_null() != 5 ) return 6;
    return 0;
}
