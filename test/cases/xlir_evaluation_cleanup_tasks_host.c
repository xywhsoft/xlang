#include "../../build/test/xlir_evaluation_cleanup_tasks.c"
int evaluation_tasks_host_init(void) { return xlir_module_init(); }
void evaluation_tasks_host_fini(void) { xlir_module_fini(); }
