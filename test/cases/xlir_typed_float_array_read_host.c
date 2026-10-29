#include "../../build/test/xlir_typed_float_array_read_generated.c"

int main(void)
{
    xlir_result_i64 result;
    if (!xlir_module_init()) return 1;
    result = xlir_fn_1();
    return result.ok && result.value == 1 ? 0 : 1;
}
