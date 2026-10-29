/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_global_scalar_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && s3 == 40 && xlir_fn_1() == 42 ? 0 : 1;
}
