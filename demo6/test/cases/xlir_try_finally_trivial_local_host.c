/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_try_finally_trivial_local_generated.c"

int main(void)
{
    if ( xlir_module_init() != 1 || xlir_fn_1() != 3 ) return 1;
    return xlir_fn_2() == 7 ? 0 : 2;
}
