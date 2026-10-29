#include "xlir_pending_callback_preparation.c"
int pending_graph_bind(const XL6_CodeOwner* owner) { return xl_script_bind_code_owner(owner); }
int pending_graph_init(void) { return xl_script_module_init(NULL); }
int pending_graph_prepare(void) { return xl_script_module_retirement_v4()->preflight(); }
int pending_graph_finish(void) { return xl_script_module_unit_checked(); }
bool pending_graph_trace(XL6_ModuleOwnershipVisit visit,void* context)
{ return xl_script_module_ownership_trace(visit,context); }
void pending_graph_keep(int64_t observed) { pending_callback_keep(observed); }
