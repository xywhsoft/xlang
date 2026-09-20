#include "../../build/test/xlir_string_return_borrowed_param_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_2() == 37 ? 0 : 1;
}
