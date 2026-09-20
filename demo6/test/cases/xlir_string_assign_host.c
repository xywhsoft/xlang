#include "../../build/test/xlir_string_assign_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1() == 23 ? 0 : 1;
}
