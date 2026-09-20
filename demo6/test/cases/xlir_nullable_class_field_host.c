#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_class_field_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_class_default() != 5 ) return 2;
    if ( xlir_nullable_class_present() != 64 ) return 3;
    if ( xlir_nullable_class_reset() != 11 ) return 4;
    if ( xlir_nullable_class_explicit() != 23 ) return 5;
    return 0;
}
