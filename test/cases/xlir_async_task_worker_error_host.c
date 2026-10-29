/* The worker assert must reject its Future through the task failure ABI. */
#include "../../build/test/xlir_async_task_worker_error_generated.c"

int main(void)
{
    return xlir_module_init() == 0 ? 0 : 1;
}
