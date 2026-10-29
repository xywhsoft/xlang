#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_index_assign_if_null_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_index_assign(1) != 8 ) return 2;
    if ( xlir_nullable_index_assign(0) != 117 ) return 3;
    return 0;
}
