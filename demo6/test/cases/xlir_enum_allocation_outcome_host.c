#include "xlir_enum_allocation_outcome.c"
int enum_outcome_init(void) { return xlir_module_init(); }
void enum_outcome_finish(void) { xlir_module_fini(); }
int64_t enum_outcome_invoke(int kind, int caught)
{
    return caught ? xl6_export_enum_allocation_caught(kind) : xl6_export_enum_allocation_case(kind);
}
