#include "../../build/test/xlir_xvalue_array_literal_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1() == 47 ? 0 : 1;
}
