/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_for_range_control_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1() == 19 ? 0 : 1;
}
