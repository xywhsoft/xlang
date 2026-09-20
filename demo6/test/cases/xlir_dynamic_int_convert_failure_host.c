/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_dynamic_int_convert_failure_generated.c"

int main(void)
{
    xlir_result_i64 result;
    if ( !xlir_module_init() ) return 1;
    result = xlir_fn_1();
    return result.ok && result.value == 73 ? 0 : 1;
}
