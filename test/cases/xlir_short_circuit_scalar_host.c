/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_short_circuit_scalar_generated.c"

int main(void)
{
    return xlir_module_init() == 1 &&
           xlir_fn_1(true, false) == false && xlir_fn_1(true, true) == true &&
           xlir_fn_2(false, false) == false && xlir_fn_2(false, true) == true ? 0 : 1;
}
