/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_dynamic_wide_int_convert_generated.c"

int main(void)
{
    xlir_result_i64 a;
    xlir_result_i64 b;
    xlir_result_i64 c;
    xlir_result_i64 d;
    if ( !xlir_module_init() ) return 1;
    a = xlir_fn_1(); b = xlir_fn_2(); c = xlir_fn_3(); d = xlir_fn_4();
    return a.ok && a.value == -32768 && b.ok && b.value == 65535 &&
           c.ok && c.value == INT64_C(-2147483648) && d.ok && d.value == INT64_C(4294967295) ? 0 : 1;
}
