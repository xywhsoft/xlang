#define XRT_MODULE_TASK_POOL
#include "lib/xrt/xrt.h"
#include "xl6_task_frame_internal.h"
#include "xl6_task_pool_internal.h"
#include "src/api/xl_foreign_error.h"
/* The legacy/public V1 ABI stays pool-specific. Only this provider references
 * pool factories, admission/activity bookkeeping and native pool submission. */
static const XL6_TaskFramePoolV1 pool_frame_binding={
    xl6TaskPoolHoldInternalV1,xl6TaskPoolDropInternalV1,xl6TaskPoolOwnership,
    xl6TaskActivityEnterInternalV1,xl6TaskActivityLeaveInternalV1
};
typedef struct PoolTarget {
    XL6_TaskPool* pool;
    xtaskpool* native;
    TaskActivity activity;
} PoolTarget;
static bool pool_begin(void* data)
{
    PoolTarget* target=data;xl6TaskActivityEnterInternalV1(&target->activity,target->pool);
    target->native=xl6TaskSubmitBeginInternalV1(target->pool);
    if(target->native)return true;
    xl6TaskActivityLeaveInternalV1(&target->activity);return false;
}
static void pool_end(void* data)
{
    PoolTarget* target=data;
    xl6TaskActivityLeaveInternalV1(&target->activity);xl6TaskSubmitEndInternalV1(target->pool);
}
static xfuture* pool_submit(void* data,void* frame,xcancel* cancel,
    XL6_FuturePayloadTrace trace,const xfuturepayloadownershipv1* result)
{
    PoolTarget* target=data;const xtaskdataownershipv1* policy=xl6TaskNativeDataPolicyV1Get();
    xtaskargs args={cancel,policy->Drop,NULL};
    return result?xrtTaskSubmitOwnedJobV1(target->native,frame,cancel,policy,result):
        trace?xrtTaskSubmitTraced(target->native,policy->Proc,frame,&args,trace):
        xrtTaskSubmit(target->native,policy->Proc,frame,&args);
}
static xfuture* task_submit_policy(XL6_TaskPool* pool,void* data,xcancel* cancel,
    XL6_FuturePayloadTrace trace,const xfuturepayloadownershipv1* policy,bool language)
{
    if(!pool){
        if(!xrtGetError()) {
            xerrordesc desc={XERR_STATE,0,0,"xlang","throw","module task resource is inactive","StateError",NULL};
            xerror* error=xrtErrorBuild(&desc);if(error)xrtSetErrorTake(error);else xrtSetErrorKind(XERR_STATE);
        }
        return NULL;
    }
    PoolTarget context={.pool=pool};
    const XL6_TaskFrameTargetV1 target={&context,pool_begin,pool_end,pool_submit,pool,&pool_frame_binding};
    xl6TaskPoolEnterInternalV1(pool);
    xfuture* future=xl6TaskFrameSubmitInternalV1(data,cancel,trace,policy,language,&target);
    xl6TaskPoolLeaveInternalV1(pool);return future;
}
static xfuture* task_submit(XL6_TaskPool* pool,void* data,xcancel* cancel,XL6_FuturePayloadTrace trace,bool language)
{ return task_submit_policy(pool,data,cancel,trace,NULL,language); }
xfuture* xl6TaskSubmitPayloadV1(XL6_TaskPool* pool,void* data,xcancel* cancel,XL6_FuturePayloadKind kind,bool language)
{
    const xfuturepayloadownershipv1* policy=xl6PayloadPolicyGet(kind);
    if(!policy){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return task_submit_policy(pool,data,cancel,NULL,policy,language);
}
xfuture* xl6TaskSubmitOwnedPolicyV1(XL6_TaskPool* pool,void* data,xcancel* cancel,
    const xfuturepayloadownershipv1* policy,bool language)
{
    if(!policy||policy->size!=sizeof(*policy)||!policy->Drop||!policy->Trace){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    return task_submit_policy(pool,data,cancel,NULL,policy,language);
}
const XL6_TaskRuntimeV1* xl6TaskRuntimeV1Get(void)
{
    static const XL6_TaskRuntimeV1 api={xl6TaskPoolCreateInternalV1,xl6TaskPoolQuiesceInternalV1,
        xl6TaskPoolDestroyInternalV1,xl6TaskPoolIsQuiescentInternalV1,xl6TaskPoolIsClosingInternalV1,
        xl6TaskFrameAllocateInternalV1,xl6TaskFrameDiscardInternalV1,xl6TaskFrameOwnershipInternalV1,task_submit};
    return &api;
}
