/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_nested_try_finally_return_scalar_generated.c"

int main(void)
{
    if ( xlir_module_init() != 1 ) return 1;
    if ( xlir_fn_1() != 47 ) return 2;
    return xlir_fn_2() == 12 ? 0 : 3;
}
