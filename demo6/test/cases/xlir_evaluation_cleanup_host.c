#include "../../build/test/xlir_evaluation_cleanup.c"
int evaluation_host_init(void) { return xlir_module_init(); }
void evaluation_host_fini(void) { xlir_module_fini(); }
int64_t evaluation_host_case(int kind) { return xl6_export_evaluation_case(kind); }
int64_t evaluation_host_operand_case(int kind) { return xl6_export_evaluation_operand_case(kind); }
