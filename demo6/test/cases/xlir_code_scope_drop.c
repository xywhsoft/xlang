#include "xlir_code_scope_drop.h"
__declspec(dllexport) void scope_generated_drop(void* value, void* data)
{
    ScopeDropHooks* hooks = (ScopeDropHooks*)data;
    (void)value;
    hooks->during(hooks->context);
    ++hooks->returned; /* Must still be mapped after the host callback returns. */
}
