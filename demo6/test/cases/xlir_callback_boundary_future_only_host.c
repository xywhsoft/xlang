#include "xlir_callback_boundary.future_only.c"
#include "xlir_callback_boundary.future_only.h.bind.h"
int callback_probe_init(void) { return xlir_module_init(); }
void callback_probe_fini(void) { xlir_module_fini(); }
xfuture* callback_probe_start(xfuture* source)
{
    xlir_result_future result = CALLBACK_THEN(source);
    return result.ok ? result.value : NULL;
}
