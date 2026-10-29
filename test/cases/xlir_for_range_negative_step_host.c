/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_for_range_negative_step_generated.c"

int main(void)
{
    xlir_result_i64 result;
    if ( xlir_module_init() != 1 ) return 1;
    result = xlir_fn_1();
    return result.ok && result.value == 9 ? 0 : 1;
}
