typedef struct xerror xerror;
#include "xlir_code_lease_drop.h"
__declspec(dllexport) void lease_generated_drop(void* value, void* data)
{
    LeaseDropHooks* hooks = (LeaseDropHooks*)data;
    hooks->inspect(hooks->context);
    /* Use the caller's XRT instance, including its error scope/allocator. A
     * C-host JIT may use the CLI's runtime rather than the published xl.dll. */
    hooks->dispose(value, hooks->error);
}
