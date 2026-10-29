#include "xl6_channel_wait_common.h"
#include "src/api/xl_foreign_error.h"
bool xl6ChannelSelectCancelV1(const XL6_ChannelCaseV1* cases,size_t count,XL6_ChannelWaitOptionsV2 options,
    xcancel* cancellation,XL6_ChannelSelectionV1* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    XL6_ChannelSelectFrameV1 frame={0}; XL6_ChannelSelectionV1 selected={SIZE_MAX,XL6_CHANNEL_TIMEOUT_V1};
    XL6_DeadlineV1 deadline=0; xcancel* token=NULL; bool ok=false;
    if (xl6ChannelWaitDeadline(options,&deadline)) {
        token=cancellation ? xrtCancelRef(cancellation) : NULL;
        if ((!cancellation || token) && xl6ChannelSelectPrepareV1(cases,count,output,&frame)) {
            xchannelselectresult native=xrtChannelSelectForCancel(frame.native,frame.count,xl6DeadlineRemainingMillisecondsV1(deadline),token);
            ok=xl6ChannelSelectCompleteV1(&frame,native,&selected);
        }
    }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6ChannelSelectDiscardV1(&frame); xrtCancelDestroy(token);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); return false; }
    *output=selected; return true;
}
