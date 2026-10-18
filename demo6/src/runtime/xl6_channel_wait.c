#include "xl6_channel_wait_common.h"
#include "src/api/xl_foreign_error.h"
static bool waitOne(const xvalue* channel,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitOptionsV2 options,XL6_ChannelStatusV1* status,bool sending)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    XL6_ChannelOperationV1 operation={0}; XL6_ChannelStatusV1 actual=XL6_CHANNEL_OK_V1;
    xdeadline deadline=0; bool ok=false;
    if (xl6ChannelWaitDeadline(options,&deadline) &&
        xl6ChannelOperationOpenV1(channel,identity,element,type,slot,sending,status,&operation)) {
        if (options.mode==XL6_CHANNEL_TRY_V1) {
            xchannelresult native=sending ? xrtChannelTrySend(operation.native,operation.message) :
                xrtChannelTryRecv(operation.native,&operation.received);
            switch (native) {
            case XCHANNEL_OK: actual=XL6_CHANNEL_OK_V1; ok=true; break;
            case XCHANNEL_CLOSED: actual=XL6_CHANNEL_CLOSED_V1; ok=true; break;
            case XCHANNEL_FULL: actual=XL6_CHANNEL_FULL_V1; ok=true; break;
            case XCHANNEL_EMPTY: actual=XL6_CHANNEL_EMPTY_V1; ok=true; break;
            default: break;
            }
        } else {
            xwaitresult native=sending ? xrtChannelSendUntil(operation.native,operation.message,deadline) :
                xrtChannelRecvUntil(operation.native,&operation.received,deadline);
            ok=xl6ChannelWaitStatus(native,sending,options.mode,&actual);
        }
        if (ok && actual==XL6_CHANNEL_OK_V1) ok=xl6ChannelOperationCommitV1(&operation);
    }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6ChannelOperationCloseV1(&operation);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); return false; }
    *status=actual; return true;
}
bool xl6ChannelSendWaitV2(const xvalue* channel,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitOptionsV2 options,XL6_ChannelStatusV1* status)
{ return waitOne(channel,identity,element,type,slot,options,status,true); }
bool xl6ChannelReceiveWaitV2(const xvalue* channel,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitOptionsV2 options,XL6_ChannelStatusV1* status)
{ return waitOne(channel,identity,element,type,slot,options,status,false); }
