#include "xlir_callback_boundary.c"
#include "xlir_callback_boundary.h.bind.h"
int callback_boundary_init(void) { return xlir_module_init(); }
void callback_boundary_fini(void) { xlir_module_fini(); }
xfuture* callback_boundary_start(unsigned kind, xfuture* source)
{
    xlir_result_future result;
    switch (kind) {
    case 0: result = CALLBACK_THEN(source); break;
    case 1: result = CALLBACK_CATCH(source); break;
    case 2: result = CALLBACK_FINALLY(source); break;
    case 4: result = CALLBACK_OWNED(source); break;
    default: result = CALLBACK_TASK(source); break;
    }
    return result.ok ? result.value : NULL;
}
