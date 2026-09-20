/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_match_string_generated.c"

int main(void)
{
    return xlir_module_init() == 1 &&
           xlir_fn_1("yes") == 10 && xlir_fn_1("no") == 20 && xlir_fn_1("other") == 30 ? 0 : 1;
}
