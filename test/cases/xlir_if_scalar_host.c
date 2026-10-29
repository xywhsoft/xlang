/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_if_scalar_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1(true) == 7 && xlir_fn_1(false) == 9 ? 0 : 1;
}
