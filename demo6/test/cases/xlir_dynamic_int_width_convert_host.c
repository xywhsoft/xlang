/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_dynamic_int_width_convert_generated.c"

int main(void)
{
    xlir_result_i64 signedResult;
    xlir_result_i64 unsignedResult;
    if ( !xlir_module_init() ) return 1;
    signedResult = xlir_fn_1();
    unsignedResult = xlir_fn_2();
    return signedResult.ok && signedResult.value == -128 &&
           unsignedResult.ok && unsignedResult.value == 255 ? 0 : 1;
}
