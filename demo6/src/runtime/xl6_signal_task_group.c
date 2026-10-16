#include "src/runtime/xl6_text_value.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_native_handle.h"

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
