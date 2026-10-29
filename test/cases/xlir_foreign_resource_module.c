#include "xlir_foreign_resource_module.h"
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
EXPORT bool foreign_module_producer(ForeignModuleHooks* hooks,const xrtcallframe* frame)
{
    hooks->dispose(frame);++hooks->stages;return true;
}
EXPORT bool foreign_module_adapter(void* data,const xrtcallframe* frame,xrtcallresult* result)
{
    (void)result;XL6_Closure* closure=data;
    ForeignModuleHooks* hooks=*(ForeignModuleHooks**)closure->environment;
    bool ok=hooks->producer(hooks,frame);++hooks->stages;return ok;
}
