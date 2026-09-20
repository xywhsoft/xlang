#include "../../build/test/xlir_string_local_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1() == 42 ? 0 : 1;
}
