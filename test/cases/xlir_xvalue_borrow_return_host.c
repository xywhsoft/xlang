#include "../../build/test/xlir_xvalue_borrow_return_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_2() == 43 ? 0 : 1;
}
