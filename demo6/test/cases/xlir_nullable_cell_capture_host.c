#include <stdbool.h>

#ifndef XLIR_GENERATED_MODULE_PREINCLUDED
#include "../../build/test/xlir_nullable_cell_capture_generated.c"
#endif

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_nullable_cell_probe(false) != 94 ) return 2;
    if ( xlir_nullable_cell_probe(true) != 49 ) return 3;
    return 0;
}
