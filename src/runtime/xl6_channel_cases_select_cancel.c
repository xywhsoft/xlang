#include "xl6_channel_case_internal.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>
XL6_ChannelSelectionOwnerV1* xl6ChannelCasesSelectCancelV1(XL6_ChannelCaseOwnerV1* const* cases,
    size_t count,XL6_ChannelWaitOptionsV2 options,xcancel* token)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    XL6_ChannelOwnedSelectFrameV1 frame={0}; XL6_ChannelSelectionV1 info={0};
    bool ok=token && xl6ChannelOwnedSelectPrepareV1(cases,count,&options,&frame) &&
        xl6ChannelSelectCancelV1(frame.cases,count,options,token,&info) && xl6ChannelOwnedSelectCompleteV1(&frame,info);
    if (!ok || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    XL6_ChannelSelectionOwnerV1* result=xl6ChannelOwnedSelectDiscardV1(&frame,ok);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ChannelSelectionDestroyV1(result); xl6ErrorScopePropagateFailure(outcome); return NULL; }
    xrtErrorFree(outcome.error); return result;
}
