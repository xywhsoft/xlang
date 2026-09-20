#include "xlir_resident_closure_dll.h"
__declspec(dllexport) bool closure_generated_invoke(void* data, int64_t* output)
{
    ClosureTestEnvironment* environment = (ClosureTestEnvironment*)data;
    ClosureTestHooks* hooks = environment->producer;
    hooks->during(environment);
    *output = 42; ++hooks->invoked;
    return true; /* Producer code must survive the host hook and this tail. */
}
__declspec(dllexport) void closure_generated_drop(void* data)
{
    ClosureTestEnvironment* environment = (ClosureTestEnvironment*)data;
    ClosureTestHooks* hooks = environment->producer;
    hooks->drop(environment); ++hooks->dropped;
}
__declspec(dllexport) bool closure_generated_trace(const void* data, XL6_ModuleOwnershipVisit visit, void* context)
{
    const ClosureTestEnvironment* environment = (const ClosureTestEnvironment*)data;
    return environment->producer->trace(environment, visit, context);
}
__declspec(dllexport) bool closure_generated_adapter(void* data, const struct xrtcallframe* frame, struct xrtcallresult* result)
{
    XL6_Closure* closure = (XL6_Closure*)data;
    ClosureTestHooks* hooks = ((ClosureTestEnvironment*)closure->environment)->adapter;
    ClosureTestInvoke invoke = (ClosureTestInvoke)closure->invoke;
    int64_t value = 0; (void)frame;
    if (!invoke(closure->environment, &value) || !hooks->result(result, value)) return false;
    ++hooks->adapted;
    return true; /* This DLL is distinct from the producer DLL. */
}
__declspec(dllexport) const char* const* closure_generated_names(void)
{ static const char* const names[] = {"amount"}; return names; }
__declspec(dllexport) const xlir_call_default* closure_generated_defaults(void)
{ static const xlir_call_default defaults[] = {{2, false, 42, 0, 0.0, NULL}}; return defaults; }
