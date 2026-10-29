/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_nested_try_finally_string_return_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_2() == 12 ? 0 : 1;
}
