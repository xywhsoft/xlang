/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_finally_nested_managed_local_generated.c"

int main(void)
{
    if ( xlir_module_init() != 1 || xlir_fn_1() != 1 ) return 1;
    return xlir_fn_2() == 5 ? 0 : 2;
}
