/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_match_scalar_generated.c"

int main(void)
{
    return xlir_module_init() == 1 &&
           xlir_fn_1(0) == 10 && xlir_fn_1(1) == 20 && xlir_fn_1(2) == 30 ? 0 : 1;
}
