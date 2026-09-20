#include "xlir_generator_storage_drop.h"
__declspec(dllexport) bool generator_storage_resume(XL6_Generator* generator)
{
    GeneratorStorageHooks* hooks=((GeneratorStorageFrame*)generator->frame)->hooks;
    ++hooks->resumed; hooks->during(hooks->context);
    hooks->api->Finish(generator); return false;
}
__declspec(dllexport) void generator_storage_frame_drop(void* data)
{
    GeneratorStorageFrame* frame=data; GeneratorStorageHooks* hooks=frame->hooks;
    ++hooks->dropped; hooks->during(hooks->context);
    hooks->api->FrameFree(frame); ++hooks->returned;
}
__declspec(dllexport) bool generator_storage_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ (void)data; (void)visit; (void)context; return true; }

/* V2 descriptors themselves live in the real DLL, not in a resident copy.
 * An independently pinned frame must keep these bytes and its return tail. */
#include "xlir_generator_managed_dll.h"
__declspec(dllexport) const XL6_ActivationCaptureLayoutV1* generator_managed_frame_layout(void)
{
    static const XL6_ActivationCaptureSlot slot={{XL6_CAPTURE_TRIVIAL,offsetof(ManagedDllFrame,hooks),sizeof(ManagedDllHooks*)},SIZE_MAX};
    static const XL6_ActivationCaptureLayoutV1 layout={sizeof(layout),sizeof(ManagedDllFrame),1,&slot,0,NULL};return &layout;
}
__declspec(dllexport) const XL6_CaptureLayoutV1* generator_managed_item_layout(void)
{
    static const XL6_CaptureSlot slot={XL6_CAPTURE_TRIVIAL,0,sizeof(int64_t)};
    static const XL6_CaptureLayoutV1 layout={sizeof(layout),sizeof(int64_t),1,&slot};return &layout;
}
__declspec(dllexport) bool generator_managed_resume(XL6_Generator* generator)
{
    ManagedDllHooks* hooks=((ManagedDllFrame*)generator->frame)->hooks;
    if(!generator->closing&&!generator->state){
        int64_t item=17;hooks->api->SetState(generator,1);return hooks->api->Put(generator,&item,sizeof(item));
    }
    ++hooks->entered;hooks->during(hooks->context);hooks->api->Finish(generator);++hooks->returned;return false;
}
