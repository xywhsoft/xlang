/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_float_to_int_checked_catch_generated.c"

int main(void)
{
    xlir_result_i64 result;
    if ( !xlir_module_init() ) return 1;
    result = xlir_fn_1();
    return result.ok && result.value == 77 ? 0 : 1;
}
