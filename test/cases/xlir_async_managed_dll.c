#include "xlir_async_managed_dll.h"
static const XL6_ActivationCaptureSlot slots[]={{{XL6_CAPTURE_TRIVIAL,offsetof(AsyncDllFrame,hooks),sizeof(AsyncDllHooks*)},SIZE_MAX}};
static const XL6_ActivationCaptureLayoutV1 captures={sizeof(captures),sizeof(AsyncDllFrame),1,slots,0,NULL};
static const XL6_ContinuationFrameLayoutV2 layout={sizeof(layout),&captures,
    offsetof(AsyncDllFrame,ticket),offsetof(AsyncDllFrame,watch),offsetof(AsyncDllFrame,source),offsetof(AsyncDllFrame,outer)};
__declspec(dllexport) const XL6_ContinuationFrameLayoutV2* async_managed_layout(void) {return &layout;}
__declspec(dllexport) void async_managed_run(void* data)
{
    AsyncDllFrame* frame=data;AsyncDllHooks* hooks=frame->hooks;
    ++hooks->entered;hooks->during(hooks->context);++hooks->returned;
}
