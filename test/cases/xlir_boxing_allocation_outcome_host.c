#include "xlir_boxing_allocation_outcome.c"
int boxing_outcome_init(void) { return xlir_module_init(); }
void boxing_outcome_finish(void) { xlir_module_fini(); }
int64_t boxing_outcome_invoke(int kind, int caught)
{
    return caught ? xl6_export_boxing_allocation_caught(kind) : xl6_export_boxing_allocation_case(kind);
}
int64_t boxing_outcome_callback(void) { return xl6_export_boxing_insertion_callback(); }
int64_t boxing_outcome_failed_insertion(void) { return xl6_export_boxing_insertion_failure(); }
