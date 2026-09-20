/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_typed_int_array_read_generated.c"

int main(void)
{
    xlir_result_i64 result;
    if ( !xlir_module_init() ) return 1;
    result = xlir_fn_1();
    return result.ok && result.value == 43 ? 0 : 1;
}
