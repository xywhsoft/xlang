/* A failed xvalue worker must never expose a value through its Future. */
#include "../../build/test/xlir_async_task_xvalue_worker_error_generated.c"

int main(void)
{
    return xlir_module_init() == 0 ? 0 : 1;
}
