/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_dynamic_wide_int_failure_generated.c"

int main(void)
{
    xlir_result_i64 a;
    xlir_result_i64 b;
    if ( !xlir_module_init() ) return 1;
    a = xlir_fn_1(); b = xlir_fn_2();
    return a.ok && a.value == 21 && b.ok && b.value == 23 ? 0 : 1;
}
