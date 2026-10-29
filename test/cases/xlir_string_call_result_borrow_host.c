#include "../../build/test/xlir_string_call_result_borrow_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_3() == 9 ? 0 : 1;
}
