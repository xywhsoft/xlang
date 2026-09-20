/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_recursive_scalar_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1(6) == 720 ? 0 : 1;
}
