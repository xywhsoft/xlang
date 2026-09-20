#include "xlir_module_mixed_dependency_artifact.c"
static int64_t (*aggregate_mode_hook)(void);
static xfuture* (*aggregate_ready_hook)(void);
__declspec(dllexport) void aggregate_dependency_bind(int64_t (*mode)(void), xfuture* (*ready)(void))
{ aggregate_mode_hook = mode; aggregate_ready_hook = ready; }
int64_t dependency_aggregate_mode(void) { return aggregate_mode_hook(); }
xfuture* dependency_aggregate_ready(void) { return aggregate_ready_hook(); }
