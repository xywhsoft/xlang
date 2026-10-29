/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_try_catch_finally_break_scalar_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_1() == 3 ? 0 : 1;
}
