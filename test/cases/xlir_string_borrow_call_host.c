#include "../../build/test/xlir_string_borrow_call_generated.c"

int main(void)
{
    xlir_result_i64 result;
    if (xlir_module_init() != 1) return 1;
    result = xlir_fn_2();
    return result.ok && result.value == 7 ? 0 : 1;
}
