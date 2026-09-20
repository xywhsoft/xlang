/* The quality gate generates this translation unit before compiling the host. */
#include "../../build/test/xlir_checked_float_string_generated.c"

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) abort();
}

int main(void)
{
    xlir_result_f64 result;
    if ( !xlir_module_init() ) return 1;
    result = xlir_fn_2();
    return result.ok && result.value == 73.0 ? 0 : 1;
}
