#include "xl6_channel_future_deadline.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef struct XL6_ChannelFutureFrameV1 Frame;
typedef struct Deadline {
    XL6_DeadlineTicketV1 ticket;
    Frame* frame; /* borrowed uniquely-containing core; source owns real RC */
    xchannelsubscription* subscription;
    const void* role;
    XL6_DeadlineV1 due;
    xerror* error;
    bool armed,failed,consumed,cleaned;
} Deadline;
static const XL6_ChannelFutureWakeV1 wake;
static Deadline* state(const Frame* frame)
{
    if(xl6ChannelFutureCoreWakePolicyV1(frame)!=&wake)return NULL;
    return xl6ChannelFutureCoreWakeDataV1(frame);
}
static void notify(const void* data,XL6_DeadlineResultV1 result,const xerror* error)
{
    Frame* frame=(Frame*)data;Deadline* deadline=state(frame);
    if(!deadline||!deadline->subscription)abort();
    if(result!=XL6_DEADLINE_EXPIRED_V1&&result!=XL6_DEADLINE_FAILED_V1)abort();
    bool failed=result==XL6_DEADLINE_FAILED_V1;
    xl6ChannelFutureCoreStopV1(frame,deadline->subscription,failed?XWAIT_ERROR:XWAIT_TIMEOUT,failed,error);
    /* No access after Stop: the worker may already have retired semantic work.
     * This source's real frame/subscription owners still pin physical tails. */
}
static xrtownershipref ownership(const void* data)
{return xl6ChannelFutureCoreOwnershipV1(data);}
static void drop_target(const void* data)
{
    Frame* frame=(Frame*)data;Deadline* deadline=state(frame);if(!deadline)abort();
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    xchannelsubscription* subscription=deadline->subscription;deadline->subscription=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    /* Release the auxiliary native credit BEFORE the source's frame credit.
     * Otherwise an aux->subscription->Data(frame) cycle could never retire. */
    xrtChannelSubscribeDestroy(subscription);xl6ChannelFutureCoreDataDropV1(frame);
}
static const XL6_DeadlineTargetV1 target={sizeof(target),notify,drop_target,ownership};
static bool arm(void* data,const void* context)
{
    Deadline* deadline=data;XL6_DeadlineServiceV1* service=(XL6_DeadlineServiceV1*)context;
    if(!service||deadline->armed||deadline->cleaned||deadline->subscription){xrtSetErrorKind(XERR_STATE);return false;}
    deadline->subscription=xl6ChannelFutureCoreSubscriptionRefV1(deadline->frame);
    if(!deadline->subscription)return false;
    bool held=xl6ChannelFutureCoreRetainV1(deadline->frame);
    if(!held)xrtSetErrorKind(XERR_RANGE);
    deadline->armed=true;const void* owner=deadline->frame;
    if(held&&xl6DeadlineBindTakeV1(service,&deadline->ticket,deadline->due,&target,&owner))return true;
    /* Bind failure moves nothing and has no notification. Preserve its first
     * rich false/NULL outcome while rolling back both prepared credits. */
    xerror* error=xrtTakeError();deadline->armed=false;
    xchannelsubscription* subscription=deadline->subscription;deadline->subscription=NULL;
    xrtChannelSubscribeDestroy(subscription);
    if(held)xl6ChannelFutureCoreDataDropV1(deadline->frame);
    xrtClearError();xrtSetErrorTake(error);return false;
}
static void detach(void* data) {xl6DeadlineDetachV1(&((Deadline*)data)->ticket);}
static void record_failure(void* data,const xerror* error)
{
    Deadline* deadline=data;if(deadline->failed)return;
    deadline->failed=true;deadline->error=xrtErrorRef(error);
}
static bool consume_failure(void* data,bool propagate)
{
    Deadline* deadline=data;
    if(!deadline->failed||deadline->consumed)return true;
    deadline->consumed=true;xerror* error=deadline->error;deadline->error=NULL;
    if(propagate){xl6ErrorScopePropagateFailure((xl6_error_result){false,error});return false;}
    xrtErrorFree(error);return true;
}
static bool take_failure(void* data,xchannelselectresult result)
{
    /* The source's error is a STOP request, not permission to undo a native
     * commit. Ready/closed or an earlier stop retain their actual arbitration
     * result; a source error that won ERROR keeps its rich false/NULL outcome.
     * Service.Check independently retains the terminal source failure. */
    return consume_failure(data,result.Wait==XWAIT_ERROR);
}
static void cleanup(void* data)
{
    Deadline* deadline=data;detach(deadline);(void)consume_failure(deadline,true);deadline->cleaned=true;
}
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const Deadline* deadline=data;
    return visit&&visit(xl6DeadlineTicketServiceOwnershipV1(&deadline->ticket),context)&&
        visit(xrtChannelSubscribeOwnership(deadline->subscription),context)&&
        visit(xrtErrorOwnership(deadline->error),context);
}
static bool empty(const void* data)
{const Deadline* deadline=data;return deadline->cleaned&&!deadline->error;}
static void drop(void* data)
{
    Deadline* deadline=data;
    if(!empty(deadline)||deadline->subscription||!xl6DeadlineTicketIsIdleV1(&deadline->ticket))abort();
    xrtFree(deadline);
}
static const XL6_ChannelFutureWakeV1 wake={sizeof(wake),arm,detach,record_failure,take_failure,cleanup,trace,empty,drop};
bool xl6ChannelFutureDeadlineAttachV1(Frame* frame,XL6_DeadlineV1 due,const void* role)
{
    if(!frame||!role||due==XL6_DEADLINE_NEVER){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    Deadline* deadline=xrtCalloc(1,sizeof(*deadline));if(!deadline)return false;
    deadline->frame=frame;deadline->due=due;deadline->role=role;void* owned=deadline;
    bool ok=xl6ChannelFutureCoreWakeTakeV1(frame,&wake,&owned);if(!ok)xrtFree(owned);return ok;
}
bool xl6ChannelFutureDeadlineMatchesV1(const Frame* frame,const void* role)
{const Deadline* deadline=state(frame);return deadline&&role&&deadline->role==role;}
xfuture* xl6ChannelFutureDeadlineStartV1(const void* role,XL6_ContinuationExecutor* executor,
    XL6_DeadlineServiceV1* service,Frame* frame,const xvalue* owner,const char* field,size_t size,xcancel* parent)
{
    if(!service||!xl6ChannelFutureDeadlineMatchesV1(frame,role)){xrtSetErrorKind(XERR_TYPE);return NULL;}
    return xl6ChannelFutureCoreStartWakeV1(executor,frame,owner,field,size,parent,service);
}
