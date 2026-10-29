#include "../../build/test/xlir_xvalue_assign_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1() == 53 ? 0 : 1;
}
