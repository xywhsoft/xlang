#include "../../build/test/xlir_checked_projection.c"
int checked_projection_host_init(void) { return xlir_module_init(); }
void checked_projection_host_fini(void) { xlir_module_fini(); }
int64_t checked_projection_host_case(int kind) { return xl6_export_checked_projection_case(kind); }
