#include "../../build/test/xlir_task_byval.c"
int task_byval_init(void) { return xlir_module_init(); }
void task_byval_fini(void) { xlir_module_fini(); }
