#include "xl6_channel_select_deadline.h"
#include "xl6_channel_future_deadline.h"
#include "src/api/xl_foreign_error.h"
static const unsigned selectRole=3;
static XL6_ChannelFutureFrameV1* prepare(XL6_ChannelCaseOwnerV1* const* cases,size_t count,
    bool relative,int64_t milliseconds,XL6_DeadlineV1 until)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return NULL;
    XL6_ChannelFutureFrameV1* frame=NULL;XL6_DeadlineV1 due=until;bool ok=false;
    if(relative){if(!xl6DeadlineFromMillisecondsV1(milliseconds,&due))goto done;}
    else if(due==XL6_DEADLINE_NEVER){xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    frame=xl6ChannelFutureRuntimeV1Get()->Prepare(cases,count);
    ok=frame&&xl6ChannelFutureDeadlineAttachV1(frame,due,&selectRole);
done:
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    if(scope.failed){xl6ChannelFutureCoreDiscardV1(frame);frame=NULL;
        if(xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);}
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return NULL;}xrtErrorFree(result.error);return frame;
}
static XL6_ChannelFutureFrameV1* prepare_for(XL6_ChannelCaseOwnerV1* const* cases,size_t count,int64_t ms)
{return prepare(cases,count,true,ms,0);}
static XL6_ChannelFutureFrameV1* prepare_until(XL6_ChannelCaseOwnerV1* const* cases,size_t count,XL6_DeadlineV1 due)
{return prepare(cases,count,false,0,due);}
static xfuture* start(XL6_ContinuationExecutor* executor,XL6_DeadlineServiceV1* service,XL6_ChannelFutureFrameV1* frame,
    const xvalue* owner,const char* field,size_t size,xcancel* parent)
{return xl6ChannelFutureDeadlineStartV1(&selectRole,executor,service,frame,owner,field,size,parent);}
const XL6_ChannelSelectDeadlineRuntimeV1* xl6ChannelSelectDeadlineRuntimeV1Get(void)
{
    static const XL6_ChannelSelectDeadlineRuntimeV1 api={prepare_for,prepare_until,xl6ChannelFutureCoreResultHandleV1,
        start,xl6ChannelFutureCoreDiscardV1,xl6ChannelFutureCoreOwnershipV1};return &api;
}
