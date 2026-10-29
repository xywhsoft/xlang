/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_dynamic_width_compare_generated.c"

int main(void)
{
    xlir_result_bool signedResult;
    xlir_result_bool unsignedResult;
    if ( !xlir_module_init() ) return 1;
    signedResult = xlir_fn_1();
    unsignedResult = xlir_fn_2();
    return signedResult.ok && signedResult.value &&
           unsignedResult.ok && unsignedResult.value ? 0 : 1;
}
