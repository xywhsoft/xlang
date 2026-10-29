#include "xlir_task_storage_drop.h"
__declspec(dllexport) XL6_TaskOutcome task_storage_run(struct xcancel* cancel,void* data,XL6_TaskResult* result)
{
    TaskStorageHooks* hooks=((TaskStoragePayload*)data)->hooks;(void)cancel;
    ++hooks->runs;hooks->during(hooks->context,0);*result=(XL6_TaskResult){0};return XL6_TASK_SUCCESS;
}
__declspec(dllexport) void task_storage_drop(void* data,void* user)
{
    TaskStorageHooks* hooks=((TaskStoragePayload*)data)->hooks;(void)user;
    ++hooks->drops;hooks->during(hooks->context,1);++hooks->returned;
}
__declspec(dllexport) bool task_storage_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ (void)data;(void)visit;(void)context;return true; }
