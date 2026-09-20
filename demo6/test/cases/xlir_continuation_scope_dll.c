#include "xlir_continuation_scope_dll.h"
/* Reuse actual generated-style closure callbacks, not host-resident stand-ins. */
#include "xlir_resident_closure_dll.c"
__declspec(dllexport) void continuation_generated_run(void* data)
{
    ContinuationScopeFrame* frame = data;
    ContinuationScopeHooks* hooks = frame->hooks;
    hooks->run(frame); ++hooks->ran;
}
__declspec(dllexport) void continuation_generated_release(void* data)
{
    ContinuationScopeFrame* frame = data;
    ContinuationScopeHooks* hooks = frame->hooks;
    hooks->release(frame); ++hooks->released;
    /* frame may be freed. This return address must remain mapped. */
}
