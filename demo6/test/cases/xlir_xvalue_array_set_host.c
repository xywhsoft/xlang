#include "../../build/test/xlir_xvalue_array_set_generated.c"

int main(void)
{
    xlir_result_i64 result;
    if ( xlir_module_init() != 1 ) return 1;
    result = xlir_fn_1();
    return result.ok && result.value == 59 ? 0 : 1;
}
