/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_async_task_scalar_generated.c"

int main(void)
{
    return xlir_module_init() == 1 && s4 == 42 ? 0 : 1;
}
