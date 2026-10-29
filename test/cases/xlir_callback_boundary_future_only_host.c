#include "xlir_callback_boundary.future_only.c"
#include "xlir_callback_boundary.future_only.h.bind.h"
int callback_probe_init(void) { return xlir_module_init(); }
void callback_probe_fini(void) { xlir_module_fini(); }
xrtownershipref callback_probe_executor(void) { return xl6ContinuationExecutorOwnership(xlir_continuation_executor); }
bool callback_probe_idle(void) { return xl6ContinuationExecutorIsIdle(xlir_continuation_executor); }
xfuture* callback_probe_start(xfuture* source)
{
    xlir_result_future result = CALLBACK_THEN(source);
    return result.ok ? result.value : NULL;
}
