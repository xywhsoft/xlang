/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_try_finally_managed_local_generated.c"

int main(void)
{
    if ( xlir_module_init() != 1 || xlir_fn_1() != 2 ) return 1;
    return xlir_fn_2() == 9 ? 0 : 2;
}
