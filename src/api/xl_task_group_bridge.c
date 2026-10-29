#include <limits.h>
#include <stdlib.h>
#define XRT_MODULE_TASK_GROUP
#include "src/common/xl6_xrt.h"
#include "xl_task_group_bridge.h"
#include "xl_clock.h"

/* Creator, owned parent Data and every in-flight operation have distinct
 * actual rights. Group remains owned until physical release or Finish. */
struct xl6_task_group {
    xmutex Lock;
    volatile int32 References;
    size_t Busy;
    xtaskgroup* Group;
    xcancelwatch* ParentWatch;
    const void* Claim;
    bool BindingParent, Preparing, Prepared, Closing, Cleared;
    uint64 Reaped;
};
static void begin(xl6_task_group* group,xrtownershipscope* scope)
{
    if(!xrtOwnershipMutationBegin(scope))abort();
    if(!xrtMutexLock(&group->Lock))abort();
}
static void end(xl6_task_group* group,xrtownershipscope* scope)
{
    if(!xrtMutexUnlock(&group->Lock) || !xrtOwnershipScopeEnd(scope))abort();
}
static void drop(const void* data)
{
    xl6_task_group* group=(xl6_task_group*)data;xtaskgroup* native;
    xrtownershipscope scope={0};begin(group,&scope);
    int32 refs=xrtRefRelease(&group->References);
    if(refs<0)abort();
    if(refs){end(group,&scope);return;}
    if(group->Busy || group->BindingParent || group->ParentWatch)abort();
    native=group->Group;group->Group=NULL;end(group,&scope);
    /* Native destruction can notify/reenter. No wrapper lock or mutation is
     * inherited; teardown cannot replace the operation's primary error. */
    xerror* prior=xrtTakeError();xrtTaskGroupDestroy(native);
    xrtClearError();xrtSetErrorTake(prior);
    if(!xrtOwnershipMutationBegin(&scope))abort();
    if(!xrtMutexUnit(&group->Lock))abort();
    xrtFree(group);
    if(!xrtOwnershipScopeEnd(&scope))abort();
}
static xtaskgroup* enter(xl6_task_group* group,bool report)
{
    if(!group){if(report)xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xrtownershipscope scope={0};xtaskgroup* native=NULL;bool overflow=false;
    begin(group,&scope);
    if(!group->Closing && !group->Cleared && group->Group){
        overflow=group->Busy==SIZE_MAX;
        if(!overflow && xrtRefRetain(&group->References)>0){++group->Busy;native=group->Group;}
        else overflow=true;
    }
    end(group,&scope);
    if(!native && report)xrtSetErrorKind(overflow?XERR_RANGE:XERR_CLOSED);
    return native;
}
static void leave(xl6_task_group* group)
{
    xrtownershipscope scope={0};begin(group,&scope);
    if(!group->Busy)abort();
    --group->Busy;end(group,&scope);drop(group);
}
static uint64 deadline_ms(long long milliseconds)
{
    if(milliseconds<=0)return 0;
    return (uint64)milliseconds;
}
static xl6_task_group* wrap(xtaskgroup* native)
{
    if(!native)return NULL;
    xl6_task_group* group=xrtCalloc(1,sizeof(*group));
    if(!group){xrtTaskGroupDestroy(native);return NULL;}
    if(!xrtMutexInit(&group->Lock)){xrtTaskGroupDestroy(native);xrtFree(group);return NULL;}
    xrtownershipscope scope={0};begin(group,&scope);
    group->References=1;group->Group=native;end(group,&scope);return group;
}
static void parent_cancelled(ptr data)
{
    xl6_task_group* group=data;xtaskgroup* native=enter(group,false);
    if(!native)return; /* Closing/cleared handles admit no new work. */
    (void)xrtTaskGroupCancel(native);leave(group);
}
static bool count(const void* data,size_t* result)
{
    const xl6_task_group* group=data;
    if(!group || !result || group->References<=0 || group->Busy || group->BindingParent ||
        group->Cleared || !group->Group)return false;
    *result=(size_t)group->References;return true;
}
static bool trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const xl6_task_group* group=data;size_t refs;
    return visit && count(data,&refs) && visit(xrtTaskGroupOwnership(group->Group),context) &&
        (!group->ParentWatch || visit(xrtCancelWatchOwnership(group->ParentWatch),context));
}
static const xrtownershipops ops={count,trace};
static bool busy(const void* data)
{
    const xl6_task_group* group=data;
    return group && !group->Cleared && (group->Busy || group->BindingParent);
}
XL_API const xrtownershipquiescencev1* xl6TaskGroupQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 policy={sizeof(policy),&ops,busy};return &policy;
}
static const xcancelwatchownershipv1 parent_policy={sizeof(parent_policy),parent_cancelled,drop,&ops};
XL_API const xcancelwatchownershipv1* xl6TaskGroupParentPolicyV1Get(void) { return &parent_policy; }
XL_API xrtownershipref xl6TaskGroupOwnershipV1(const xl6_task_group* group)
{ return (xrtownershipref){group,group?&ops:NULL}; }
static bool hold(const void* data)
{
    xl6_task_group* group=(xl6_task_group*)data;xrtownershipscope scope={0};bool held;
    begin(group,&scope);held=!group->Cleared && xrtRefRetain(&group->References)>0;
    end(group,&scope);return held;
}
static bool claim(const void* data,const void* token)
{
    xl6_task_group* group=(xl6_task_group*)data;size_t refs;
    if(!token || !count(data,&refs) || (group->Claim && group->Claim!=token))return false;
    group->Claim=token;return true;
}
static void restore(const void* data,const void* token)
{
    xl6_task_group* group=(xl6_task_group*)data;
    if(!token || group->Claim!=token || group->Cleared)abort();
    group->Claim=NULL;
}
static void clear(const void* data,const void* token)
{
    xl6_task_group* group=(xl6_task_group*)data;
    if(!token || group->Claim!=token || group->Cleared || group->Busy || group->BindingParent)abort();
    group->Cleared=true;group->Closing=true; /* Preserve all owning slots. */
}
static xrtownershipfinishresult finish(const void* data,const void* token)
{
    xl6_task_group* group=(xl6_task_group*)data;xrtownershipscope scope={0};begin(group,&scope);
    if(!token || group->Claim!=token || !group->Cleared || group->Busy || group->BindingParent)abort();
    xcancelwatch* watch=group->ParentWatch;end(group,&scope);
    if(watch){
        if(!xrtCancelUnwatchChecked(watch))return XRT_OWNERSHIP_FINISH_FAILED;
        begin(group,&scope);group->ParentWatch=NULL;end(group,&scope);
    }
    begin(group,&scope);xtaskgroup* native=group->Group;group->Group=NULL;end(group,&scope);
    /* Global Clear has completed. Consume the wrapper's native creator right,
     * never the native node's independent plan pin or a semantic cancellation. */
    xrtTaskGroupDestroy(native);return XRT_OWNERSHIP_FINISH_READY;
}
XL_API const xrtownershipadapterv1* xl6TaskGroupOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershipfinishv1** completion)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,NULL};
    static const xrtownershipfinishv1 completed={sizeof(completed),&adapter,finish};size_t refs;
    if(!completion || ref.Ops!=&ops || !count(ref.Data,&refs) || ((const xl6_task_group*)ref.Data)->Closing)return NULL;
    *completion=&completed;return &adapter;
}
/* Preparation performs the ordinary language owner's Close/cooperative-Cancel
 * semantics without consuming its creator right. Execution owners still drain
 * callbacks/finally; parent registration is retired only after all items end. */
static bool prepared(const void* data)
{
    const xl6_task_group* group=data;
    return group->Prepared && !group->Busy && !group->BindingParent && !group->ParentWatch;
}
static xrtownershipprepareresult prepare(const void* data,const void* token)
{
    xl6_task_group* group=(xl6_task_group*)data;xrtownershipscope scope={0};
    begin(group,&scope);
    if(!token || group->Claim!=token || group->Cleared)abort();
    if(group->Busy || group->BindingParent){end(group,&scope);return XRT_OWNERSHIP_PREPARE_BUSY;}
    group->Preparing=true;xtaskgroup* native=group->Group;end(group,&scope);
    (void)xrtTaskGroupClose(native);
    xtaskgroupstats stats;
    if(!xrtTaskGroupGet(native,&stats))return XRT_OWNERSHIP_PREPARE_FAILED;
    if(stats.Active){
        if(!xrtTaskGroupCancel(native) && (!xrtTaskGroupGet(native,&stats) || !stats.Cancelling))
            return XRT_OWNERSHIP_PREPARE_FAILED;
        return XRT_OWNERSHIP_PREPARE_BUSY;
    }
    begin(group,&scope);xcancelwatch* watch=group->ParentWatch;end(group,&scope);
    if(watch){
        if(!xrtCancelUnwatchChecked(watch))return XRT_OWNERSHIP_PREPARE_FAILED;
        begin(group,&scope);group->ParentWatch=NULL;end(group,&scope);
    }
    begin(group,&scope);
    group->Prepared=!group->Busy && !group->BindingParent && !group->ParentWatch;
    bool ready=prepared(group);end(group,&scope);
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void clear_prepared(const void* data,const void* token)
{ if(!prepared(data))abort();clear(data,token); }
XL_API const xrtownershipadapterv1* xl6TaskGroupOwnershipAdapterV2(xrtownershipref ref,
    const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear_prepared,NULL};
    static const xrtownershippreparationv1 preparation_api={sizeof(preparation_api),&adapter,prepared,prepare};
    static const xrtownershipfinishv1 completed={sizeof(completed),&adapter,finish};size_t refs;
    if(!preparation || !completion || ref.Ops!=&ops || !count(ref.Data,&refs))return NULL;
    *preparation=&preparation_api;*completion=&completed;return &adapter;
}
static bool config(const XL6_TaskGroupConfigV1* input,xtaskgroupconfig* output)
{
    *output=(xtaskgroupconfig){0};
    if(!input)return true;
    if(input->size!=sizeof(*input) || input->limit>SIZE_MAX ||
        (input->cancelOn&~XRT_TASK_GROUP_CANCEL_ON_STOPPED)) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    output->Cancel=input->cancel;output->Limit=(size_t)input->limit;output->CancelOn=input->cancelOn;
    return true;
}
XL_API xl6_task_group* xl6TaskGroupCreateConfiguredV1(const XL6_TaskGroupConfigV1* input)
{
    xtaskgroupconfig native;
    return config(input,&native)?wrap(xrtTaskGroupCreate(&native)):NULL;
}
XL_API xl6_task_group* xl6TaskGroupCreate(void) { return xl6TaskGroupCreateConfiguredV1(NULL); }
XL_API xl6_task_group* xl6TaskGroupChildConfiguredV1(xl6_task_group* parent,const XL6_TaskGroupConfigV1* input)
{
    xtaskgroupconfig options;if(!config(input,&options))return NULL;
    xtaskgroup* native=enter(parent,true);if(!native)return NULL;
    xl6_task_group* child=wrap(xrtTaskGroupChild(native,&options));leave(parent);return child;
}
XL_API xl6_task_group* xl6TaskGroupChild(xl6_task_group* parent)
{ return xl6TaskGroupChildConfiguredV1(parent,NULL); }
XL_API xfuture* xl6TaskGroupStartV1(xl6_task_group* group,XL6_TaskGroupStartProcV1 start,void* data)
{
    if(!start){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xtaskgroup* native=enter(group,true);if(!native)return NULL;
    xfuture* future=xrtTaskGroupStart(native,start,data);leave(group);return future;
}
static bool output_slot(const xl6_task_group* group,const void* output,size_t bytes,size_t align)
{
    uintptr_t address=(uintptr_t)output;
    if(!output || address%align || address>UINTPTR_MAX-bytes ||
        (group && address<(uintptr_t)group+sizeof(*group) && (uintptr_t)group<address+bytes)) {
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return true;
}
XL_API bool xl6TaskGroupGetV1(xl6_task_group* group,XL6_TaskGroupStatsV1* output)
{
    if(!output_slot(group,output,sizeof(*output),_Alignof(XL6_TaskGroupStatsV1)))return false;
    xtaskgroup* native=enter(group,true);if(!native)return false;
    xtaskgroupstats stats;bool ok=xrtTaskGroupGet(native,&stats);
    XL6_TaskGroupStatsV1 value={0};
    if(ok){
        value=(XL6_TaskGroupStatsV1){stats.Active,stats.Added,stats.Completed,stats.Succeeded,
            stats.Failed,stats.Cancelled,stats.Closed,stats.Rejected,
            stats.FirstIndex==SIZE_MAX?0:stats.FirstIndex,(int32_t)stats.FirstState,
            stats.FirstIndex!=SIZE_MAX,stats.Accepting,stats.Cancelling};
    }
    leave(group);if(ok)*output=value;return ok;
}
XL_API bool xl6TaskGroupErrorCopyV1(xl6_task_group* group,xerror** output)
{
    if(!output_slot(group,output,sizeof(*output),_Alignof(xerror*)) || *output){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    xtaskgroup* native=enter(group,true);if(!native)return false;
    const xerror* borrowed=xrtTaskGroupError(native);xerror* error=borrowed?xrtErrorRef((xerror*)borrowed):NULL;
    bool ok=!borrowed||error;leave(group);if(ok)*output=error;return ok;
}
XL_API bool xl6TaskGroupCancellationV1(xl6_task_group* group,xcancel** output)
{
    if(!output_slot(group,output,sizeof(*output),_Alignof(xcancel*)) || *output){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    xtaskgroup* native=enter(group,true);if(!native)return false;
    xcancel* token=xrtTaskGroupCancelToken(native);leave(group);
    if(!token)return false;
    *output=token;return true;
}
XL_API bool xl6TaskGroupReapV1(xl6_task_group* group,uint64_t* output)
{
    if(!output_slot(group,output,sizeof(*output),_Alignof(uint64_t)))return false;
    xtaskgroup* native=enter(group,true);if(!native)return false;
    xtaskgroupstats stats;bool ok=xrtTaskGroupGet(native,&stats);uint64_t completed=0;
    if(ok){
        xrtownershipscope scope={0};begin(group,&scope);
        if(stats.Completed>=group->Reaped){completed=stats.Completed-group->Reaped;group->Reaped=stats.Completed;}
        end(group,&scope);
    }
    leave(group);if(ok)*output=completed;return ok;
}
XL_API int32_t xl6TaskGroupWaitV1(xl6_task_group* group,uint64_t deadline,xcancel* cancel)
{
    xtaskgroup* native=enter(group,true);if(!native)return XWAIT_ERROR;
    xwaitresult result=xrtTaskGroupWaitForCancel(native,xl6DeadlineRemainingMillisecondsV1((XL6_DeadlineV1)deadline),cancel);
    leave(group);return (int32_t)result;
}
XL_API void xl6TaskGroupDestroy(xl6_task_group* group)
{
    if(!group)return;
    xrtownershipscope scope={0};begin(group,&scope);
    if(group->Cleared){end(group,&scope);drop(group);return;}
    if(group->Closing){end(group,&scope);return;}
    group->Closing=true;xcancelwatch* watch=group->ParentWatch;group->ParentWatch=NULL;
    xtaskgroup* native=group->Group;end(group,&scope);
    /* The creator right protects this call through reentrant notification,
     * even if Unwatch synchronously releases its owned Data right. */
    xrtCancelUnwatch(watch);(void)xrtTaskGroupClose(native);
    xtaskgroupstats stats;
    if(xrtTaskGroupGet(native,&stats) && stats.Active)(void)xrtTaskGroupCancel(native);
    drop(group);
}
XL_API bool xl6TaskGroupClose(xl6_task_group* group)
{
    xtaskgroup* native=enter(group,true);if(!native)return false;
    bool result=xrtTaskGroupClose(native);leave(group);return result;
}
XL_API bool xl6TaskGroupCancel(xl6_task_group* group)
{
    xtaskgroup* native=enter(group,true);if(!native)return false;
    bool result=xrtTaskGroupCancel(native);leave(group);return result;
}
XL_API bool xl6TaskGroupBindParent(xl6_task_group* group,xfuture* parent)
{
    if(!parent){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!enter(group,true))return false;
    xrtownershipscope scope={0};begin(group,&scope);
    bool available=!group->ParentWatch && !group->BindingParent && !group->Preparing;
    if(available)group->BindingParent=true;
    end(group,&scope);
    if(!available){leave(group);xrtSetErrorKind(XERR_STATE);return false;}
    xcancel* cancel=xrtFutureCancelToken(parent);xcancelwatch* watch=NULL;
    if(cancel){
        begin(group,&scope);bool retained=xrtRefRetain(&group->References)>0;end(group,&scope);
        if(retained){
            watch=xrtCancelWatchOwnedV1(cancel,group,&parent_policy);
            if(!watch)drop(group); /* Failure did not take Data. */
        }else xrtSetErrorKind(XERR_RANGE);
        xrtCancelDestroy(cancel);
    }
    begin(group,&scope);
    available=watch && !group->Closing && !group->Cleared && !group->ParentWatch;
    if(available)group->ParentWatch=watch;
    group->BindingParent=false;end(group,&scope);
    if(watch && !available){xrtCancelUnwatch(watch);xrtSetErrorKind(XERR_CLOSED);}
    leave(group);return available;
}
XL_API bool xl6TaskGroupAdd(xl6_task_group* group,xfuture* future)
{
    xtaskgroup* native=enter(group,true);if(!native)return false;
    bool result=xrtTaskGroupAdd(native,future);leave(group);return result;
}
XL_API int xl6TaskGroupCount(xl6_task_group* group)
{
    xtaskgroup* native=enter(group,true);if(!native)return -1;
    xtaskgroupstats stats;bool ok=xrtTaskGroupGet(native,&stats);leave(group);
    return !ok?-1:stats.Active<=(size_t)INT_MAX?(int)stats.Active:INT_MAX;
}
XL_API int xl6TaskGroupReap(xl6_task_group* group)
{
    xtaskgroup* native=enter(group,true);if(!native)return -1;
    xtaskgroupstats stats;bool ok=xrtTaskGroupGet(native,&stats);uint64 result=0;
    if(ok){xrtownershipscope scope={0};begin(group,&scope);
        if(stats.Completed>=group->Reaped){result=stats.Completed-group->Reaped;group->Reaped=stats.Completed;}
        end(group,&scope);
    }
    leave(group);return !ok?-1:result<=(uint64)INT_MAX?(int)result:INT_MAX;
}
XL_API xfuture* xl6TaskGroupFuture(xl6_task_group* group)
{
    xtaskgroup* native=enter(group,true);if(!native)return NULL;
    xfuture* result=xrtTaskGroupFuture(native);leave(group);return result;
}
XL_API bool xl6TaskGroupJoin(xl6_task_group* group)
{
    xtaskgroup* native=enter(group,true);if(!native)return false;
    bool result=xrtTaskGroupWait(native)==XWAIT_OK;leave(group);return result;
}
XL_API bool xl6TaskGroupJoinFor(xl6_task_group* group,long long milliseconds)
{
    xtaskgroup* native=enter(group,true);if(!native)return false;
    bool result=xrtTaskGroupWaitFor(native,milliseconds)==XWAIT_OK;leave(group);return result;
}
XL_API bool xl6TaskGroupJoinUntil(xl6_task_group* group,long long milliseconds)
{
    xtaskgroup* native=enter(group,true);if(!native)return false;
    bool result=xrtTaskGroupWaitFor(native,xl6DeadlineRemainingMillisecondsV1(deadline_ms(milliseconds)))==XWAIT_OK;leave(group);return result;
}
