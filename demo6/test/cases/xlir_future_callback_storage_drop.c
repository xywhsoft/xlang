#include "xlir_future_callback_storage_drop.h"
static FutureCallbackStorageHooks* childHooks;
__declspec(dllexport) void future_callback_storage_proc(const struct xfutureresult* input,
    struct xpromise* output,struct xcancel* cancel,void* data)
{
    FutureCallbackStorageHooks* h=((FutureCallbackStorageData*)data)->hooks;(void)input;(void)cancel;
    ++h->calls;childHooks=h;h->during(h->context,0);h->complete(output,h->source);
}
__declspec(dllexport) void future_callback_storage_child_proc(const struct xfutureresult* input,
    struct xpromise* output,struct xcancel* cancel,void* data)
{
    FutureCallbackStorageHooks* h=((FutureCallbackStorageData*)data)->hooks;(void)input;(void)cancel;
    ++h->children;h->complete(output,h->source);
}
__declspec(dllexport) void future_callback_storage_drop(void* data,void* user)
{
    FutureCallbackStorageHooks* h=((FutureCallbackStorageData*)data)->hooks;(void)user;
    ++h->drops;h->during(h->context,1);++h->returned;
}
__declspec(dllexport) void future_callback_storage_child(void) { ++childHooks->childCalls; }
__declspec(dllexport) bool future_callback_storage_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ (void)data;(void)visit;(void)context;return true; }
