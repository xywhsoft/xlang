#include "../../build/test/xlir_match_cleanup.c"
int match_cleanup_host_init(void) { return xlir_module_init(); }
void match_cleanup_host_fini(void) { xlir_module_fini(); }
int64_t match_cleanup_host_case(int kind) { return xl6_export_match_cleanup_case(kind); }
