#include "xl6_channel_case_internal.h"
#include "xl6_channel_wait_common.h"
#include "src/api/xl_foreign_error.h"
#include <stdatomic.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

enum { CASE_NODE=1, SELECTION_NODE=2 };
typedef struct Node {
    int32_t refs;
    unsigned kind;
    size_t active,allocation;
    const void* claim;
    bool creator,cleared,dropping,dropped;
} Node;
struct XL6_ChannelCaseOwnerV1 {
    Node node;
    xvalue* channel;
    XL6_SequenceLayoutV1 layout;
    XL6_SequenceAdmissionV3 admission;
    void* value;
    unsigned operation;
    bool valueInitialized;
};
struct XL6_ChannelSelectionOwnerV1 {
    Node node;
    XL6_ChannelCaseOwnerV1* selected;
    XL6_SequenceLayoutV1 layout;
    XL6_ChannelSelectionV1 info;
    void* value;
    bool valueInitialized;
};
static _Atomic(uint32_t) gate;
static void release(Node*);
static void enter(xrtownershipscope* mutation)
{
    if (!xrtOwnershipMutationBegin(mutation)) abort();
    uint32_t expected=0;
    while (!atomic_compare_exchange_weak_explicit(&gate,&expected,1,memory_order_acq_rel,memory_order_acquire)) {
        expected=0; xrtThreadYield();
    }
}
static void leave(xrtownershipscope* mutation)
{
    atomic_store_explicit(&gate,0,memory_order_release);
    if (!xrtOwnershipScopeEnd(mutation)) abort();
}
static bool alignedSize(size_t size,size_t alignment,size_t* output)
{
    if (!alignment || alignment>_Alignof(max_align_t) || (alignment&(alignment-1)) ||
        size>SIZE_MAX-(alignment-1)) return false;
    *output=(size+alignment-1)&~(alignment-1); return true;
}
static bool region(const void* pointer,size_t size)
{ return pointer && (uintptr_t)pointer<=UINTPTR_MAX-size; }
static bool overlaps(const void* left,size_t ls,const void* right,size_t rs)
{ return (uintptr_t)left<(uintptr_t)right+rs && (uintptr_t)right<(uintptr_t)left+ls; }
static bool slotMatches(XL6_SequenceLayoutV1 layout,XL6_ChannelSlotV1 slot)
{
    return layout.type && layout.size==slot.size && region(slot.data,slot.size) &&
        layout.type->Align && (uintptr_t)slot.data%layout.type->Align==0;
}
static bool emptySlot(XL6_SequenceLayoutV1 layout,const void* slot)
{
    return layout.transfer==3 ? *(xvalue* const*)slot==NULL : layout.transfer==2 ?
        !((const xstrview*)slot)->Data && !((const xstrview*)slot)->Size : true;
}
static Node* begin(const Node* raw,unsigned kind)
{
    Node* node=(Node*)raw; xrtownershipscope mutation={0}; enter(&mutation);
    bool ok=node && node->kind==kind && node->refs>0 && node->refs<INT32_MAX &&
        !node->cleared && !node->claim && !node->dropping && node->active<SIZE_MAX;
    if (ok) { ++node->refs; ++node->active; }
    leave(&mutation);
    if (!ok) xrtSetErrorKind(XERR_STATE);
    return ok ? node : NULL;
}
static void end(Node* node,bool retainReference)
{
    xrtownershipscope mutation={0}; enter(&mutation);
    if (!node->active) abort();
    --node->active; leave(&mutation);
    if (!retainReference) release(node);
}
/* Every edge duty is detached before any destructor/callback. A failed duty
 * is reported once; retry after a GC Finish failure never double-drops it. */
static bool dispose(Node* node)
{
    xrtownershipscope mutation={0}; enter(&mutation);
    if (node->dropped) { leave(&mutation); return true; }
    if (node->active || node->dropping) abort();
    node->dropped=true; node->dropping=true;
    xvalue* channel=NULL; XL6_ChannelCaseOwnerV1* selected=NULL;
    void* value=NULL; XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 admission={0};
    if (node->kind==CASE_NODE) {
        XL6_ChannelCaseOwnerV1* item=(XL6_ChannelCaseOwnerV1*)node;
        channel=item->channel; item->channel=NULL; layout=item->layout;
        admission=item->admission; memset(&item->admission,0,sizeof(item->admission));
        if (item->valueInitialized) { value=item->value; item->valueInitialized=false; }
    } else if (node->kind==SELECTION_NODE) {
        XL6_ChannelSelectionOwnerV1* item=(XL6_ChannelSelectionOwnerV1*)node;
        selected=item->selected; item->selected=NULL; layout=item->layout;
        if (item->valueInitialized) { value=item->value; item->valueInitialized=false; }
    } else abort();
    leave(&mutation);
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBeginCleanup(&scope,false)) abort();
    if (value) xrtTypeDropValue(xl6SequenceStorageTypeV1(layout.transfer,layout.type),value);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(channel);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&admission);
    if (selected) release(&selected->node);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    enter(&mutation); node->dropping=false; leave(&mutation);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if (!result.ok) xl6ErrorScopePropagateFailure(result); else xrtErrorFree(result.error);
    return result.ok;
}
static void release(Node* node)
{
    if (!node) return;
    xrtownershipscope mutation={0}; enter(&mutation);
    if (node->refs<=0) abort();
    bool last=--node->refs==0;
    if (last) {
        if (node->creator || node->active || node->dropping || (node->claim && !node->cleared)) abort();
        node->cleared=true;
    }
    leave(&mutation);
    if (last) { (void)dispose(node); xrtFree(node); }
}
static void destroy(Node* node,unsigned kind)
{
    if (!node) return;
    xrtownershipscope mutation={0}; enter(&mutation);
    if (node->kind!=kind || !node->creator) abort();
    node->creator=false; leave(&mutation); release(node);
}
void xl6ChannelCaseDestroyV1(XL6_ChannelCaseOwnerV1* item)
{ destroy(item ? &item->node : NULL,CASE_NODE); }
void xl6ChannelSelectionDestroyV1(XL6_ChannelSelectionOwnerV1* item)
{ destroy(item ? &item->node : NULL,SELECTION_NODE); }

static XL6_ChannelCaseOwnerV1* create(const xvalue* channel,uint64_t identity,uint64_t element,
    const xrttype* type,XL6_ChannelSlotV1 slot,bool sending)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    XL6_SequenceLayoutV1 layout={0}; XL6_SequenceAdmissionV3 admission={0};
    XL6_ChannelCaseOwnerV1* item=NULL; size_t offset=0;
    if (!xl6ChannelSchemaCopyV1(channel,identity,&layout,&admission)) goto fail;
    if (layout.elementIdentity!=element || layout.type!=type || layout.size!=slot.size) {
        xrtSetErrorKind(XERR_TYPE); goto fail;
    }
    if (!alignedSize(sizeof(*item),_Alignof(max_align_t),&offset) || layout.transfer<1 || layout.transfer>3 ||
        !type->Align || type->Align>_Alignof(max_align_t) || (type->Align&(type->Align-1)) ||
        (sending && (!slotMatches(layout,slot) || !admission.ops->Slot(admission.data,slot.data))) ||
        (sending && offset>SIZE_MAX-layout.size)) { xrtSetErrorKind(XERR_ARGUMENT); goto fail; }
    size_t bytes=offset+(sending ? layout.size : 0);
    item=xrtCalloc(1,bytes); if (!item) goto fail;
    item->node=(Node){.refs=1,.kind=CASE_NODE,.allocation=bytes,.creator=true};
    item->layout=layout; item->admission=admission; memset(&admission,0,sizeof(admission));
    item->channel=xrtValueRetain(channel); if (!item->channel) goto fail;
    item->operation=sending ? XL6_CHANNEL_CASE_SEND_V1 : XL6_CHANNEL_CASE_RECEIVE_V1;
    if (sending) {
        item->value=(unsigned char*)item+offset; item->valueInitialized=true;
        if (!xrtTypeCopyValue(xl6SequenceStorageTypeV1(layout.transfer,type),item->value,slot.data)) goto fail;
    }
    goto done;
fail:
    xl6ErrorScopeCaptureFailure(&scope); xl6ChannelCaseDestroyV1(item); item=NULL;
done:
    xl6SequenceAdmissionDropV3(&admission);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if (!result.ok) { xl6ChannelCaseDestroyV1(item); xl6ErrorScopePropagateFailure(result); return NULL; }
    xrtErrorFree(result.error); return item;
}
XL6_ChannelCaseOwnerV1* xl6ChannelSendCaseCreateV1(const xvalue* channel,uint64_t identity,
    uint64_t element,const xrttype* type,XL6_ChannelSlotV1 slot)
{ return create(channel,identity,element,type,slot,true); }
XL6_ChannelCaseOwnerV1* xl6ChannelReceiveCaseCreateV1(const xvalue* channel,uint64_t identity,
    uint64_t element,const xrttype* type,size_t size)
{ return create(channel,identity,element,type,(XL6_ChannelSlotV1){NULL,size},false); }

bool xl6ChannelOwnedSelectPrepareV1(XL6_ChannelCaseOwnerV1* const* inputs,size_t count,
    XL6_ChannelWaitOptionsV2* options,XL6_ChannelOwnedSelectFrameV1* frame)
{
    xdeadline deadline=0;
    if (!frame || frame->count || frame->cases || frame->pins || frame->slots || frame->result ||
        !options || !count || count>=UINT32_MAX || count>SIZE_MAX/sizeof(*inputs) ||
        count>SIZE_MAX/sizeof(*frame->cases) || !region(inputs,count*sizeof(*inputs)) ||
        (uintptr_t)inputs%_Alignof(XL6_ChannelCaseOwnerV1*)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!xl6ChannelWaitDeadline(*options,&deadline)) return false;
    /* Fix a relative deadline before metadata capture/allocation/copy. */
    if (options->mode==XL6_CHANNEL_FOR_V1)
        *options=(XL6_ChannelWaitOptionsV2){sizeof(*options),XL6_CHANNEL_UNTIL_V2,0,deadline};
    if (count<=8) { frame->cases=frame->inlineCases; frame->pins=frame->inlinePins; }
    else {
        frame->cases=xrtCalloc(count,sizeof(*frame->cases)); if (!frame->cases) return false;
        frame->pins=xrtCalloc(count,sizeof(*frame->pins)); if (!frame->pins) return false;
    }
    size_t bytes=0,maximum=0;
    for (size_t i=0;i<count;++i) {
        XL6_ChannelCaseOwnerV1* item=inputs[i];
        if (!begin(item ? &item->node : NULL,CASE_NODE)) return false;
        frame->pins[i]=item; frame->count=i+1;
        frame->cases[i]=(XL6_ChannelCaseV1){item->channel,item->layout.identity,item->layout.elementIdentity,
            item->layout.type,{item->value,item->layout.size},item->operation};
        if (item->operation==XL6_CHANNEL_CASE_RECEIVE_V1) {
            size_t aligned;
            if (!alignedSize(item->layout.size,_Alignof(max_align_t),&aligned) || bytes>SIZE_MAX-aligned) {
                xrtSetErrorKind(XERR_RANGE); return false;
            }
            bytes+=aligned; if (maximum<item->layout.size) maximum=item->layout.size;
        }
    }
    size_t offset;
    if (!alignedSize(sizeof(*frame->result),_Alignof(max_align_t),&offset) || offset>SIZE_MAX-maximum) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    frame->result=xrtCalloc(1,offset+maximum); if (!frame->result) return false;
    frame->result->node=(Node){.refs=1,.kind=SELECTION_NODE,.allocation=offset+maximum,.creator=true};
    frame->result->value=(unsigned char*)frame->result+offset;
    if (bytes) {
        frame->slots=bytes<=sizeof(frame->inlineSlots) ? frame->inlineSlots : xrtCalloc(1,bytes);
        if (!frame->slots) return false;
    }
    size_t at=0;
    for (size_t i=0;i<count;++i) if (frame->cases[i].operation==XL6_CHANNEL_CASE_RECEIVE_V1) {
        size_t aligned; if (!alignedSize(frame->cases[i].slot.size,_Alignof(max_align_t),&aligned)) abort();
        frame->cases[i].slot.data=frame->slots+at; at+=aligned;
    }
    return true;
}
bool xl6ChannelOwnedSelectCompleteV1(XL6_ChannelOwnedSelectFrameV1* frame,XL6_ChannelSelectionV1 info)
{
    if (!frame || !frame->result || frame->result->selected ||
        (info.index==SIZE_MAX ? info.status!=XL6_CHANNEL_TIMEOUT_V1 && info.status!=XL6_CHANNEL_CANCELLED_V1 :
         info.index>=frame->count || (info.status!=XL6_CHANNEL_OK_V1 && info.status!=XL6_CHANNEL_CLOSED_V1))) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    XL6_ChannelSelectionOwnerV1* result=frame->result; result->info=info;
    if (info.index!=SIZE_MAX) {
        XL6_ChannelCaseOwnerV1* item=frame->pins[info.index]; result->layout=item->layout;
        if (info.status==XL6_CHANNEL_OK_V1 && item->operation==XL6_CHANNEL_CASE_RECEIVE_V1) {
            memcpy(result->value,frame->cases[info.index].slot.data,item->layout.size);
            memset(frame->cases[info.index].slot.data,0,item->layout.size); result->valueInitialized=true;
        }
        /* Transfer the selected operation's real reference, never allocate or
         * perform a fallible post-commit retain. Other case credits end below. */
        xrtownershipscope mutation={0}; enter(&mutation);
        result->selected=item; frame->pins[info.index]=NULL;
        if (!item->node.active) abort();
        --item->node.active; leave(&mutation);
    }
    return true;
}
XL6_ChannelSelectionOwnerV1* xl6ChannelOwnedSelectDiscardV1(XL6_ChannelOwnedSelectFrameV1* frame,bool success)
{
    if (!frame) return NULL;
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBeginCleanup(&scope,false)) abort();
    for (size_t i=0;i<frame->count;++i) {
        XL6_ChannelCaseV1* item=frame->cases+i;
        if (item->operation==XL6_CHANNEL_CASE_RECEIVE_V1 && item->slot.data) {
            xrtTypeDropValue(xl6SequenceStorageTypeV1(frame->pins[i] ? frame->pins[i]->layout.transfer :
                frame->result->layout.transfer,item->type),item->slot.data);
            if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
        }
        if (frame->pins[i]) end(&frame->pins[i]->node,false);
        if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    }
    if (frame->slots && frame->slots!=frame->inlineSlots) xrtFree(frame->slots);
    if (frame->pins && frame->pins!=frame->inlinePins) xrtFree(frame->pins);
    if (frame->cases && frame->cases!=frame->inlineCases) xrtFree(frame->cases);
    XL6_ChannelSelectionOwnerV1* result=frame->result;
    if (!success) { xl6ChannelSelectionDestroyV1(result); result=NULL; }
    memset(frame,0,sizeof(*frame));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome); else xrtErrorFree(outcome.error);
    return result;
}

bool xl6ChannelSelectionInfoV1(const XL6_ChannelSelectionOwnerV1* raw,XL6_ChannelSelectionV1* output)
{
    if (!output || (uintptr_t)output%_Alignof(XL6_ChannelSelectionV1) || !region(output,sizeof(*output)) ||
        (raw && overlaps(output,sizeof(*output),raw,raw->node.allocation))) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    Node* node=begin(raw ? &raw->node : NULL,SELECTION_NODE); if (!node) return false;
    *output=raw->info; end(node,false); return true;
}
bool xl6ChannelSelectionValueV1(const XL6_ChannelSelectionOwnerV1* raw,const XL6_ChannelCaseOwnerV1* expected,
    uint64_t element,const xrttype* type,XL6_ChannelSlotV1 output)
{
    Node* node=begin(raw ? &raw->node : NULL,SELECTION_NODE); if (!node) return false;
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    bool ok=false; void* copy=NULL; XL6_SequenceLayoutV1 layout=raw->layout;
    _Alignas(max_align_t) unsigned char inlineCopy[64]={0};
    if (!expected || raw->selected!=expected || raw->info.status!=XL6_CHANNEL_OK_V1 ||
        !raw->valueInitialized || expected->operation!=XL6_CHANNEL_CASE_RECEIVE_V1) xrtSetErrorKind(XERR_STATE);
    else if (raw->layout.elementIdentity!=element || raw->layout.type!=type || output.size!=raw->layout.size)
        xrtSetErrorKind(XERR_TYPE);
    else if (!slotMatches(raw->layout,output) || overlaps(output.data,output.size,raw,raw->node.allocation) ||
        overlaps(output.data,output.size,expected,expected->node.allocation) || !emptySlot(raw->layout,output.data))
        xrtSetErrorKind(XERR_ARGUMENT);
    else {
        copy=output.size<=sizeof(inlineCopy) ? inlineCopy : xrtCalloc(1,output.size);
        if (copy && xrtTypeCopyValue(xl6SequenceStorageTypeV1(raw->layout.transfer,type),copy,raw->value)) {
            ok=true;
        }
    }
    if (!ok || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    end(node,false);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    /* Last-creator cleanup can fail too. Publish only AFTER that tail; false
     * never leaks an already-written owned value to a caller's result slot. */
    if (copy && scope.failed) {
        xrtTypeDropValue(xl6SequenceStorageTypeV1(layout.transfer,layout.type),copy);
        memset(copy,0,output.size);
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    }
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if (result.ok && copy) { memcpy(output.data,copy,output.size); memset(copy,0,output.size); }
    if (copy && copy!=inlineCopy) xrtFree(copy);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); return false; }
    xrtErrorFree(result.error); return true;
}
static bool count(const void* raw,size_t* output)
{
    const Node* node=raw;
    if (!node || !output || node->refs<=0 || node->active || node->cleared || node->dropping) return false;
    *output=(size_t)node->refs; return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const Node* node=raw; size_t refs;
    if (!visit || !count(raw,&refs)) return false;
    if (node->kind==CASE_NODE) {
        const XL6_ChannelCaseOwnerV1* item=raw;
        return (!item->channel || visit(xrtValueOwnership(item->channel),context)) &&
            (!item->valueInitialized || item->layout.transfer!=3 || visit(xrtValueOwnership(*(xvalue**)item->value),context));
    }
    if (node->kind==SELECTION_NODE) {
        const XL6_ChannelSelectionOwnerV1* item=raw;
        return (!item->selected || visit(xl6ChannelCaseOwnershipV1(item->selected),context)) &&
            (!item->valueInitialized || item->layout.transfer!=3 || visit(xrtValueOwnership(*(xvalue**)item->value),context));
    }
    return false;
}
static const xrtownershipops caseOps={count,trace},selectionOps={count,trace};
xrtownershipref xl6ChannelCaseOwnershipV1(const XL6_ChannelCaseOwnerV1* item)
{ return (xrtownershipref){item,&caseOps}; }
xrtownershipref xl6ChannelSelectionOwnershipV1(const XL6_ChannelSelectionOwnerV1* item)
{ return (xrtownershipref){item,&selectionOps}; }
static bool caseHandleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    return visit && xrtValueGetHandle(value,&raw,&ops,&user) && raw && !user &&
        ops==xl6ChannelCaseHandlePolicyV1Get()->Ops && visit(xl6ChannelCaseOwnershipV1(raw),context);
}
static bool selectionHandleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    return visit && xrtValueGetHandle(value,&raw,&ops,&user) && raw && !user &&
        ops==xl6ChannelSelectionHandlePolicyV1Get()->Ops && visit(xl6ChannelSelectionOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelCaseHandlePolicyV1Get,xl6ChannelCaseDestroyV1,caseHandleTrace)
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ChannelSelectionHandlePolicyV1Get,xl6ChannelSelectionDestroyV1,selectionHandleTrace)
static bool hold(const void* raw)
{ Node* node=(Node*)raw; if (node->refs<=0 || node->refs==INT32_MAX) return false; ++node->refs; return true; }
static void drop(const void* raw) { release((Node*)raw); }
static bool claim(const void* raw,const void* token)
{
    Node* node=(Node*)raw;
    if (!token || node->cleared || node->active || node->dropping || (node->claim && node->claim!=token)) return false;
    node->claim=token; return true;
}
static void restore(const void* raw,const void* token)
{ Node* node=(Node*)raw; if (node->claim!=token || node->cleared) abort(); node->claim=NULL; }
static void clear(const void* raw,const void* token)
{
    Node* node=(Node*)raw;
    if (node->claim!=token || node->cleared || node->active || node->dropping) abort();
    node->cleared=true;
}
static bool finish(const void* raw,const void* token)
{ Node* node=(Node*)raw; if (node->claim!=token || !node->cleared) abort(); return dispose(node); }
const xrtownershipadapterv1* xl6ChannelCaseOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,finish};
    size_t refs;
    return ((ref.Ops==&caseOps && ref.Data && ((const Node*)ref.Data)->kind==CASE_NODE) ||
        (ref.Ops==&selectionOps && ref.Data && ((const Node*)ref.Data)->kind==SELECTION_NODE)) && count(ref.Data,&refs) ? &adapter : NULL;
}
static bool busy(const void* raw)
{ const Node* node=raw; return node && node->refs>0 && (node->active || node->dropping); }
const xrtownershipquiescencev1* xl6ChannelCaseQuiescenceV1Get(void)
{ static const xrtownershipquiescencev1 value={sizeof(value),&caseOps,busy}; return &value; }
const xrtownershipquiescencev1* xl6ChannelSelectionQuiescenceV1Get(void)
{ static const xrtownershipquiescencev1 value={sizeof(value),&selectionOps,busy}; return &value; }
