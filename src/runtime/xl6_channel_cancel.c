#include "xl6_channel_wait_common.h"
#include "src/api/xl_foreign_error.h"
static bool cancelOne(const xvalue* channel,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitOptionsV2 options,xcancel* cancellation,
    XL6_ChannelStatusV1* status,bool sending)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    XL6_ChannelOperationV1 operation={0}; XL6_ChannelStatusV1 actual=XL6_CHANNEL_OK_V1;
    XL6_DeadlineV1 deadline=0; xcancel* token=NULL; bool ok=false;
    if (xl6ChannelWaitDeadline(options,&deadline)) {
        token=cancellation ? xrtCancelRef(cancellation) : NULL;
        if ((!cancellation || token) &&
            xl6ChannelOperationOpenV1(channel,identity,element,type,slot,sending,status,&operation)) {
            /* No ownership admission spans native registration or waiting.
             * Native Unwatch completes its callback tail before returning. */
            xwaitresult native=sending ? xrtChannelSendForCancel(operation.native,operation.message,xl6DeadlineRemainingMillisecondsV1(deadline),token) :
                xrtChannelRecvForCancel(operation.native,&operation.received,xl6DeadlineRemainingMillisecondsV1(deadline),token);
            ok=xl6ChannelWaitStatus(native,sending,options.mode,&actual);
            if (ok && actual==XL6_CHANNEL_OK_V1) ok=xl6ChannelOperationCommitV1(&operation);
        }
    }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6ChannelOperationCloseV1(&operation); xrtCancelDestroy(token);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); return false; }
    *status=actual; return true;
}
bool xl6ChannelSendCancelV2(const xvalue* channel,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitOptionsV2 options,xcancel* token,XL6_ChannelStatusV1* status)
{ return cancelOne(channel,identity,element,type,slot,options,token,status,true); }
bool xl6ChannelReceiveCancelV2(const xvalue* channel,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitOptionsV2 options,xcancel* token,XL6_ChannelStatusV1* status)
{ return cancelOne(channel,identity,element,type,slot,options,token,status,false); }
