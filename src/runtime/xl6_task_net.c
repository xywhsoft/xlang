#define XRT_MODULE_TASK_NET
#include "lib/xrt/xrt.h"
#include "xl6_task_net.h"
#include "xl6_task_frame_internal.h"
/* Proc has no second result/cleanup implementation: the shared task frame
 * executes its checked transaction and the native Task validates/publishes. */
static xtaskoutcome task_net_run(xnetworker* worker,xcancel* cancel,void* frame,xtaskvalue* result)
{
    (void)worker;
    return xl6TaskNativeDataPolicyV1Get()->Proc(cancel,frame,result);
}
static void task_net_drop(void* frame,void* unused)
{xl6TaskNativeDataPolicyV1Get()->Drop(frame,unused);}
static const xtasknetdataownershipv1 data_policy={
    sizeof(data_policy),task_net_run,task_net_drop,&xl6TaskFrameOwnershipOpsInternalV1
};
const xtasknetdataownershipv1* xl6TaskNetDataPolicyV1Get(void)
{return &data_policy;}
typedef struct NetTarget {
    xnetengine* engine;
    uint64 affinity;
    bool delayed;
    xdeadline deadline;
} NetTarget;
static xfuture* submit(void* raw,void* frame,xcancel* cancel,
    XL6_FuturePayloadTrace trace,const xfuturepayloadownershipv1* result)
{
    NetTarget* target=raw;(void)trace;
    return target->delayed?xrtTaskNetUntilDataPolicyV1(target->engine,target->affinity,frame,cancel,
        &data_policy,result,target->deadline):
        xrtTaskNetDataPolicyV1(target->engine,target->affinity,frame,cancel,&data_policy,result);
}
xfuture* xl6TaskNetSubmitOwnedPolicyV1(xnetengine* engine,uint64_t affinity,void* data,
    xcancel* cancel,const xfuturepayloadownershipv1* result,bool delayed,uint64_t deadline)
{
    if(!engine||!result||result->size!=sizeof(*result)||!result->Drop||!result->Trace){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    NetTarget context={engine,affinity,delayed,deadline};
    const XL6_TaskFrameTargetV1 target={.context=&context,.Submit=submit};
    return xl6TaskFrameSubmitInternalV1(data,cancel,NULL,result,true,&target);
}
