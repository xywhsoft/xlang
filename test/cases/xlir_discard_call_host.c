/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_discard_call_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && xlir_fn_2() == 2 ? 0 : 1;
}
