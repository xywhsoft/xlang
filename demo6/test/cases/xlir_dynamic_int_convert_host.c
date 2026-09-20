/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_dynamic_int_convert_generated.c"

int main(void)
{
    xlir_result_i64 stringResult;
    xlir_result_i64 boolResult;
    xlir_result_i64 floatResult;
    if ( !xlir_module_init() ) return 1;
    stringResult = xlir_fn_1();
    boolResult = xlir_fn_2();
    floatResult = xlir_fn_3();
    return stringResult.ok && stringResult.value == 18088 &&
           boolResult.ok && boolResult.value == 1 &&
           floatResult.ok && floatResult.value == 12 ? 0 : 1;
}
