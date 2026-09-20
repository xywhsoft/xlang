#include "xlir_code_finalizer_drop.h"
__declspec(dllexport) void finalizer_generated_finalize(struct xvalue* value, void* data)
{
    FinalizerHooks* hooks = (FinalizerHooks*)data;
    hooks->finalize(value, hooks->context);
    ++hooks->finalizedReturns;
}
__declspec(dllexport) bool finalizer_generated_trace(const void* data, XL6_ModuleOwnershipVisit visit, void* context)
{
    const FinalizerHooks* hooks = (const FinalizerHooks*)data;
    return hooks->trace(hooks->context, visit, context);
}
__declspec(dllexport) void finalizer_generated_release(void* data)
{
    FinalizerHooks* hooks = (FinalizerHooks*)data;
    hooks->release(hooks->context);
    ++hooks->releasedReturns;
}
__declspec(dllexport) void finalizer_generated_field(void* data, void* unused)
{
    FinalizerHooks* hooks = (FinalizerHooks*)data;
    (void)unused;
    hooks->field(hooks->context);
    ++hooks->fieldReturns;
}
