#include "../../build/test/xlir_string_return_borrowed_param_generated.c"

int main(void)
{
    xlir_result_i64 result;
    if (xlir_module_init() != 1) return 1;
    result = xlir_fn_2();
    return result.ok && result.value == 37 ? 0 : 1;
}
