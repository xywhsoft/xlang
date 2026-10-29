#include "../../build/test/xlir_owned_projection.c"
int projection_host_init(void) { return xlir_module_init(); }
void projection_host_fini(void) { xlir_module_fini(); }
int64_t projection_host_case(int kind) { return xl6_export_projection_case(kind); }
