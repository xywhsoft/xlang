#include "../../build/test/xlir_string_ops_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1() == 17 ? 0 : 1;
}
