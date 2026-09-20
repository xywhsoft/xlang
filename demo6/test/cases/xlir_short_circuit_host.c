#include "../../build/test/xlir_short_circuit_generated.c"

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    if ( xlir_fn_21() != false ) return 1;
    if ( xlir_fn_22() != true ) return 1;
    return 0;
}
