#include "../../build/test/xlir_string_borrow_call_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_2() == 7 ? 0 : 1;
}
