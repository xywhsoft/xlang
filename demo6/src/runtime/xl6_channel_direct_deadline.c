#include "xl6_channel_direct_deadline.h"
#include "xl6_channel_future_deadline.h"
#include "src/api/xl_foreign_error.h"
static const unsigned sendRole=1,receiveRole=2;
static XL6_ChannelFutureFrameV1* prepare(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value,
    bool sending,bool relative,int64_t milliseconds,xdeadline until)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))return NULL;
    XL6_ChannelFutureFrameV1* frame=NULL;xdeadline due=until;bool ok=false;
    if(relative){if(!xl6DeadlineFromMillisecondsV1(milliseconds,&due))goto done;}
    else if(due==XRT_DEADLINE_NEVER){xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    const XL6_ChannelDirectFutureRuntimeV1* direct=xl6ChannelDirectFutureRuntimeV1Get();
    frame=sending?direct->PrepareSend(layout,channel,value):direct->PrepareReceive(layout,channel);
    ok=frame&&xl6ChannelFutureDeadlineAttachV1(frame,due,sending?&sendRole:&receiveRole);
done:
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    if(scope.failed){xl6ChannelFutureCoreDiscardV1(frame);frame=NULL;
        if(xrtGetError()||scope.failed)xl6ErrorScopeCaptureFailure(&scope);}
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return NULL;}xrtErrorFree(result.error);return frame;
}
static XL6_ChannelFutureFrameV1* send_for(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value,int64_t ms)
{return prepare(layout,channel,value,true,true,ms,0);}
static XL6_ChannelFutureFrameV1* send_until(XL6_SequenceLayoutV1 layout,const xvalue* channel,const void* value,xdeadline due)
{return prepare(layout,channel,value,true,false,0,due);}
static XL6_ChannelFutureFrameV1* receive_for(XL6_SequenceLayoutV1 layout,const xvalue* channel,int64_t ms)
{return prepare(layout,channel,NULL,false,true,ms,0);}
static XL6_ChannelFutureFrameV1* receive_until(XL6_SequenceLayoutV1 layout,const xvalue* channel,xdeadline due)
{return prepare(layout,channel,NULL,false,false,0,due);}
static xfuture* start_send(XL6_ContinuationExecutor* executor,XL6_DeadlineServiceV1* service,XL6_ChannelFutureFrameV1* frame,xcancel* parent)
{return xl6ChannelFutureDeadlineStartV1(&sendRole,executor,service,frame,NULL,NULL,0,parent);}
static xfuture* start_receive(XL6_ContinuationExecutor* executor,XL6_DeadlineServiceV1* service,XL6_ChannelFutureFrameV1* frame,
    const xvalue* owner,const char* field,size_t size,xcancel* parent)
{return xl6ChannelFutureDeadlineStartV1(&receiveRole,executor,service,frame,owner,field,size,parent);}
const XL6_ChannelDirectDeadlineRuntimeV1* xl6ChannelDirectDeadlineRuntimeV1Get(void)
{
    static const XL6_ChannelDirectDeadlineRuntimeV1 api={send_for,send_until,receive_for,receive_until,
        xl6ChannelFutureCoreResultHandleV1,start_send,start_receive,xl6ChannelFutureCoreDiscardV1,xl6ChannelFutureCoreOwnershipV1};return &api;
}
