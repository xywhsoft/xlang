/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_float_to_int_scalar_generated.c"

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    xlir_result_i64 result = xlir_fn_1();
    return result.ok && result.value == 12 ? 0 : 1;
}
