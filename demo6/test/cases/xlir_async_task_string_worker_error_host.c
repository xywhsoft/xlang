/* The worker assertion must reject its managed-result Future. */
#include "../../build/test/xlir_async_task_string_worker_error_generated.c"

int main(void)
{
    return xlir_module_init() == 0 ? 0 : 1;
}
