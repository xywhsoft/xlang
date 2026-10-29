#include "src/runtime/xl6_text_value.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_native_handle.h"

_Static_assert(XRT_TASK_GROUP_CANCEL_ON_FAILED==1 && XRT_TASK_GROUP_CANCEL_ON_CANCELLED==2 &&
    XRT_TASK_GROUP_CANCEL_ON_CLOSED==4 && XRT_TASK_GROUP_CANCEL_ON_STOPPED==7,"TaskGroup policy constants");
_Static_assert(XFUTURE_PENDING==0 && XFUTURE_RESOLVED==1 && XFUTURE_FAILED==2 &&
    XFUTURE_CANCELLED==3 && XFUTURE_CLOSED==4,"TaskGroup snapshot terminal constants");

/* The signal route retains the ordinary managed TaskGroup object. These
 * checked entries merely expose its existing resident bridge, not a new
 * scheduler or a raw pointer captured by XRT's signal dispatcher. */
void* xl6SignalTaskGroupCreateV1(void)
{
    void* group=xl6TaskGroupCreate();
    if(!group)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return group;
}
void xl6SignalTaskGroupDestroyV1(void* group)
{
    xerror* prior=xrtTakeError();xl6TaskGroupDestroy(group);
    xrtClearError();xrtSetErrorTake(prior);
}
static bool taskGroupTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;
    return visit && xrtValueGetHandle(value,&raw,NULL,NULL) && raw && visit(xl6TaskGroupOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TaskGroupHandlePolicyV1Get,xl6SignalTaskGroupDestroyV1,taskGroupTrace)
bool xl6SignalTaskGroupCloseV1(void* group)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return false;}return xl6TaskGroupClose(group); }
bool xl6SignalTaskGroupCancelV1(void* group)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return false;}return xl6TaskGroupCancel(group); }

/* These are ordinary resident methods of the existing TaskGroup class, not
 * Cgen helpers. A dynamic non-Future is an unsuccessful admission, whereas
 * native allocation/resource failure is a checked error (even without an
 * allocated diagnostic). No owning-graph certificate is granted here. */
static bool taskGroupAdmission(void* group,const xvalue* value,bool parent)
{
    if(!group){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!value || !xrtValueIsFuture(value))return false;
    xfuture* future=xrtValueGetFuture(value);
    if(!future){xrtSetErrorKind(XERR_TYPE);return false;}
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return false;
    }
    bool accepted=parent ? xl6TaskGroupBindParent(group,future) : xl6TaskGroupAdd(group,future);
    if(!accepted){
        xerrkind kind=xrtGetError()?xrtErrorKind(xrtGetError()):XERR_NONE;
        if((parent && kind==XERR_STATE) || (!parent && (kind==XERR_CLOSED || kind==XERR_AGAIN)))xrtClearError();
        else xl6ErrorScopeCaptureFailure(&scope);
    }else if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return false;}
    return accepted;
}
void* xl6_xrt_task_group_wrap(void* group) { return group; }
void* xl6_xrt_task_group_child(void* group)
{
    void* child=NULL;
    if(!group)xrtSetErrorKind(XERR_ARGUMENT);else child=xl6TaskGroupChild(group);
    if(!child)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return child;
}
bool xl6_xrt_task_group_add(void* group,const xvalue* future)
{ return taskGroupAdmission(group,future,false); }
bool xl6_xrt_task_group_bind_parent(void* group,const xvalue* future)
{ return taskGroupAdmission(group,future,true); }
int xl6_xrt_task_group_count(void* group)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return -1;}return xl6TaskGroupCount(group); }
int xl6_xrt_task_group_reap(void* group)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return -1;}return xl6TaskGroupReap(group); }
bool xl6_xrt_task_group_join(void* group)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return false;}return xl6TaskGroupJoin(group); }
bool xl6_xrt_task_group_join_timeout(void* group,int64_t milliseconds)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return false;}return xl6TaskGroupJoinFor(group,milliseconds); }
bool xl6_xrt_task_group_join_until(void* group,int64_t milliseconds)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return false;}return xl6TaskGroupJoinUntil(group,milliseconds); }
bool xl6_xrt_task_group_wait(void* group) { return xl6_xrt_task_group_join(group); }
bool xl6_xrt_task_group_wait_timeout(void* group,int64_t milliseconds)
{ return xl6_xrt_task_group_join_timeout(group,milliseconds); }
bool xl6_xrt_task_group_wait_until(void* group,int64_t milliseconds)
{ return xl6_xrt_task_group_join_until(group,milliseconds); }

/* The managed public API never exposes a borrowed/owning point. Native
 * factories transfer their creator right through the ordinary checked native
 * field contract. The snapshot is a separate immutable POD allocation, not a
 * retained group, token, Future or error hidden behind a leaf certificate. */
static bool taskGroupOptions(int64_t limit,int64_t policy,void* cancel,XL6_TaskGroupConfigV1* output)
{
    if(limit<0 || policy<0 || (uint64_t)limit>SIZE_MAX ||
        ((uint64_t)policy&~UINT64_C(7))){xrtSetErrorKind(XERR_RANGE);return false;}
    *output=(XL6_TaskGroupConfigV1){sizeof(*output),cancel,(uint64_t)limit,(uint32_t)policy};return true;
}
void* xl6TaskGroupCreateLanguageV1(int64_t limit,int64_t policy)
{
    XL6_TaskGroupConfigV1 options;
    return taskGroupOptions(limit,policy,NULL,&options)?xl6TaskGroupCreateConfiguredV1(&options):NULL;
}
void* xl6TaskGroupCreateCancelLanguageV1(void* cancel,int64_t limit,int64_t policy)
{
    XL6_TaskGroupConfigV1 options;if(!cancel){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    return taskGroupOptions(limit,policy,cancel,&options)?xl6TaskGroupCreateConfiguredV1(&options):NULL;
}
void* xl6TaskGroupChildLanguageV1(void* group,int64_t limit,int64_t policy)
{
    XL6_TaskGroupConfigV1 options;
    return taskGroupOptions(limit,policy,NULL,&options)?xl6TaskGroupChildConfiguredV1(group,&options):NULL;
}
bool xl6TaskGroupValidLanguageV1(void* group) { return group!=NULL; }
uint64_t xl6TaskGroupCountLanguageV1(void* group)
{ XL6_TaskGroupStatsV1 stats={0};return xl6TaskGroupGetV1(group,&stats)?stats.active:0; }
uint64_t xl6TaskGroupReapLanguageV1(void* group)
{ uint64_t count=0;(void)xl6TaskGroupReapV1(group,&count);return count; }
void* xl6TaskGroupTokenLanguageV1(void* group)
{ xcancel* token=NULL;(void)xl6TaskGroupCancellationV1(group,&token);return token; }
void xl6TaskGroupThrowLanguageV1(void* group)
{
    xerror* error=NULL;
    if(!xl6TaskGroupErrorCopyV1(group,&error)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return;
    }
    if(error)xl6ErrorScopePropagateFailure((xl6_error_result){false,error});
}
void* xl6TaskGroupSnapshotLanguageV1(void* group)
{
    XL6_TaskGroupStatsV1 stats={0};if(!xl6TaskGroupGetV1(group,&stats))return NULL;
    XL6_TaskGroupStatsV1* result=xrtMalloc(sizeof(*result));if(result)*result=stats;return result;
}
void xl6TaskGroupSnapshotDestroyV1(void* snapshot) { xrtFree(snapshot); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TaskGroupSnapshotHandlePolicyV1Get,xl6TaskGroupSnapshotDestroyV1)
#define XL6_GROUP_STAT(Type,Name,Field) \
    Type xl6TaskGroupSnapshot##Name##V1(void* raw) { \
        if(!raw){xrtSetErrorKind(XERR_ARGUMENT);return 0;} \
        return ((const XL6_TaskGroupStatsV1*)raw)->Field; \
    }
XL6_GROUP_STAT(uint64_t,Active,active)
XL6_GROUP_STAT(uint64_t,Added,added)
XL6_GROUP_STAT(uint64_t,Completed,completed)
XL6_GROUP_STAT(uint64_t,Succeeded,succeeded)
XL6_GROUP_STAT(uint64_t,Failed,failed)
XL6_GROUP_STAT(uint64_t,Cancelled,cancelled)
XL6_GROUP_STAT(uint64_t,Closed,closed)
XL6_GROUP_STAT(uint64_t,Rejected,rejected)
XL6_GROUP_STAT(uint64_t,FirstIndex,firstIndex)
XL6_GROUP_STAT(int64_t,FirstState,firstState)
XL6_GROUP_STAT(bool,HasFirst,hasFirst)
XL6_GROUP_STAT(bool,Accepting,accepting)
XL6_GROUP_STAT(bool,Cancelling,cancelling)
#undef XL6_GROUP_STAT
static bool taskGroupWaitResult(int32_t state)
{
    if(state==XWAIT_ERROR)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return state==XWAIT_OK;
}
static bool taskGroupTimeout(int64_t milliseconds,uint64_t* deadline)
{
    if(milliseconds<0){
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    *deadline=xl6DeadlineAfterMillisecondsV1((uint64_t)milliseconds);return true;
}
bool xl6TaskGroupUntilLanguageV1(void* group,uint64_t deadline)
{ return taskGroupWaitResult(xl6TaskGroupWaitV1(group,deadline,NULL)); }
bool xl6TaskGroupTimeoutLanguageV1(void* group,int64_t milliseconds)
{ uint64_t deadline;return taskGroupTimeout(milliseconds,&deadline)&&xl6TaskGroupUntilLanguageV1(group,deadline); }
bool xl6TaskGroupWaitCancelLanguageV1(void* group,void* cancel)
{
    if(!cancel){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return taskGroupWaitResult(xl6TaskGroupWaitV1(group,XL6_DEADLINE_NEVER,cancel));
}
bool xl6TaskGroupUntilCancelLanguageV1(void* group,uint64_t deadline,void* cancel)
{
    if(!cancel){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return taskGroupWaitResult(xl6TaskGroupWaitV1(group,deadline,cancel));
}
bool xl6TaskGroupTimeoutCancelLanguageV1(void* group,int64_t milliseconds,void* cancel)
{
    uint64_t deadline;
    return taskGroupTimeout(milliseconds,&deadline)&&xl6TaskGroupUntilCancelLanguageV1(group,deadline,cancel);
}
