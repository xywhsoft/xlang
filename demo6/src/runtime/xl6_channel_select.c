#include "xl6_channel_wait_common.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
static bool region(const void* data,size_t bytes)
{ return data && (uintptr_t)data<=UINTPTR_MAX-bytes; }
static bool intersects(const void* left,size_t ls,const void* right,size_t rs)
{ return (uintptr_t)left<(uintptr_t)right+rs && (uintptr_t)right<(uintptr_t)left+ls; }
bool xl6ChannelSelectPrepareV1(const XL6_ChannelCaseV1* cases,size_t count,
    XL6_ChannelSelectionV1* result,XL6_ChannelSelectFrameV1* frame)
{
    if (!frame || frame->count || frame->operations || frame->native || !count || count>=UINT32_MAX ||
        count>SIZE_MAX/sizeof(*cases) || count>SIZE_MAX/sizeof(*frame->operations) || count>SIZE_MAX/sizeof(*frame->native) ||
        !region(cases,count*sizeof(*cases)) || (uintptr_t)cases%_Alignof(XL6_ChannelCaseV1) ||
        !region(result,sizeof(*result)) || (uintptr_t)result%_Alignof(XL6_ChannelSelectionV1) ||
        intersects(cases,count*sizeof(*cases),result,sizeof(*result))) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    /* Validate all cross-case output ranges before allocation, owner activity,
     * copies or native publication. Shared read-only send input is legal. */
    for (size_t i=0;i<count;++i) {
        const XL6_ChannelCaseV1* item=cases+i;
        if (item->operation>XL6_CHANNEL_CASE_RECEIVE_V1 || !item->slot.size || !region(item->slot.data,item->slot.size) ||
            intersects(item->slot.data,item->slot.size,result,sizeof(*result)) ||
            intersects(item->slot.data,item->slot.size,cases,count*sizeof(*cases))) {
            xrtSetErrorKind(XERR_ARGUMENT); return false;
        }
        for (size_t j=0;j<i;++j)
            if ((item->operation==XL6_CHANNEL_CASE_RECEIVE_V1 || cases[j].operation==XL6_CHANNEL_CASE_RECEIVE_V1) &&
                intersects(item->slot.data,item->slot.size,cases[j].slot.data,cases[j].slot.size)) {
                xrtSetErrorKind(XERR_ARGUMENT); return false;
            }
    }
    if (count<=sizeof(frame->inlineOperations)/sizeof(*frame->inlineOperations)) {
        frame->operations=frame->inlineOperations; frame->native=frame->inlineNative;
    } else {
        frame->operations=xrtCalloc(count,sizeof(*frame->operations));
        if (!frame->operations) return false;
        frame->native=xrtCalloc(count,sizeof(*frame->native));
        if (!frame->native) return false;
    }
    for (size_t i=0;i<count;++i) {
        const XL6_ChannelCaseV1* item=cases+i; XL6_ChannelStatusV1 status;
        XL6_ChannelOperationV1* operation=frame->operations+i;
        if (!xl6ChannelOperationOpenV1(item->channel,item->identity,item->element,item->type,item->slot,
            item->operation==XL6_CHANNEL_CASE_SEND_V1,&status,operation)) return false;
        frame->count=i+1;
        frame->native[i]=operation->sending ? xrtChannelCaseSend(operation->native,operation->message) :
            xrtChannelCaseRecv(operation->native,&operation->received);
    }
    return true;
}
bool xl6ChannelSelectParkV1(XL6_ChannelSelectFrameV1* frame)
{
    if (!frame || !frame->count || !frame->operations || !frame->native) { xrtSetErrorKind(XERR_STATE); return false; }
    /* Validate the entire private preparation before returning ANY activity. */
    for (size_t i=0;i<frame->count;++i)
        if (!frame->operations[i].owner || frame->operations[i].parked) { xrtSetErrorKind(XERR_STATE); return false; }
    for (size_t i=0;i<frame->count;++i)
        if (!xl6ChannelOperationParkV1(frame->operations+i)) abort();
    return true;
}
bool xl6ChannelSelectTraceV1(const XL6_ChannelSelectFrameV1* frame,xrtownershipvisitor visit,ptr context)
{
    if (!frame || !visit || (frame->count && !frame->operations)) return false;
    for (size_t i=0;i<frame->count;++i) {
        const XL6_ChannelOperationV1* operation=frame->operations+i;
        if (!operation->owner || !operation->parked ||
            !visit(xl6ChannelOperationOwnershipV1(operation),context)) return false;
    }
    return true;
}
bool xl6ChannelSelectCompleteV1(XL6_ChannelSelectFrameV1* frame,xchannelselectresult native,
    XL6_ChannelSelectionV1* output)
{
    if (!frame || !output || !frame->operations || !frame->native || !frame->count) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    XL6_ChannelSelectionV1 result={SIZE_MAX,XL6_CHANNEL_TIMEOUT_V1};
    switch (native.Wait) {
    case XWAIT_OK:
        /* A committed transfer must have an exact case identity. This is a
         * trusted native protocol invariant, not a user/resource error. */
        if (native.Index>=frame->count) abort();
        result.index=native.Index;
        /* Select's Wait=OK denotes a chosen case, including CLOSED. The
         * operation outcome is a separate native Result, not Wait=CLOSED. */
        if (native.Result!=XCHANNEL_OK && native.Result!=XCHANNEL_CLOSED) { xrtSetErrorKind(XERR_STATE); return false; }
        result.status=native.Result==XCHANNEL_OK ? XL6_CHANNEL_OK_V1 : XL6_CHANNEL_CLOSED_V1;
        if (native.Result==XCHANNEL_OK && !xl6ChannelOperationCommitV1(frame->operations+native.Index)) return false;
        break;
    case XWAIT_TIMEOUT: break;
    case XWAIT_CANCELLED: result.status=XL6_CHANNEL_CANCELLED_V1; break;
    default: return false;
    }
    *output=result; return true;
}
void xl6ChannelSelectDiscardV1(XL6_ChannelSelectFrameV1* frame)
{
    if (!frame) return;
    size_t count=frame->count; XL6_ChannelOperationV1* operations=frame->operations;
    xchannelcase* native=frame->native;
    frame->count=0; frame->operations=NULL; frame->native=NULL;
    for (size_t i=0;i<count;++i) xl6ChannelOperationCloseV1(operations+i);
    if (native!=frame->inlineNative) xrtFree(native);
    if (operations!=frame->inlineOperations) xrtFree(operations);
    memset(frame,0,sizeof(*frame));
}
bool xl6ChannelSelectV1(const XL6_ChannelCaseV1* cases,size_t count,XL6_ChannelWaitOptionsV2 options,
    XL6_ChannelSelectionV1* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    XL6_ChannelSelectFrameV1 frame={0}; XL6_ChannelSelectionV1 selected={SIZE_MAX,XL6_CHANNEL_TIMEOUT_V1};
    xdeadline deadline=0; bool ok=false;
    if (xl6ChannelWaitDeadline(options,&deadline) && xl6ChannelSelectPrepareV1(cases,count,output,&frame)) {
        xchannelselectresult native=options.mode==XL6_CHANNEL_TRY_V1 ? xrtChannelSelectTry(frame.native,frame.count) :
            xrtChannelSelectUntil(frame.native,frame.count,deadline);
        ok=xl6ChannelSelectCompleteV1(&frame,native,&selected);
    }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6ChannelSelectDiscardV1(&frame);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); return false; }
    *output=selected; return true;
}
