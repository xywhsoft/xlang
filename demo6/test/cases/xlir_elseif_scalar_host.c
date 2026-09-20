/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_elseif_scalar_generated.c"

int main(void)
{
    return xlir_module_init() == 1 &&
           xlir_fn_1(-4) == -1 && xlir_fn_1(0) == 0 && xlir_fn_1(6) == 1 ? 0 : 1;
}
