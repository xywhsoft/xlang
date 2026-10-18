#include "xl6_channel_internal.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_concurrent_deadline.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct ChannelBox ChannelBox;
struct ChannelBox { ChannelBox* next; ChannelBox* previous; void* slot; };
typedef struct ChannelOwner {
    volatile int32_t refs;
    xchannel channel;
    XL6_SequenceLayoutV1 layout;
    uint64_t schemaIdentity;
    XL6_SequenceAdmissionV3 admission;
    xatomic32 activity;
    xspinlock boxesLock;
    ChannelBox* boxes;
    const void* claim;
    bool initialized, cleared, retired;
} ChannelOwner;
static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static void lockBoxes(ChannelOwner* owner)
{ if (!xrtSpinLock(&owner->boxesLock)) abort(); }
static void unlockBoxes(ChannelOwner* owner)
{ if (!xrtSpinUnlock(&owner->boxesLock)) abort(); }
static void linkBox(ChannelOwner* owner,ChannelBox* box)
{
    xrtownershipscope scope={0}; mutationBegin(&scope); lockBoxes(owner);
    box->next=owner->boxes; box->previous=NULL;
    if (box->next) box->next->previous=box;
    owner->boxes=box;
    unlockBoxes(owner); mutationEnd(&scope);
}
static void unlinkBox(ChannelOwner* owner,ChannelBox* box)
{
    xrtownershipscope scope={0}; mutationBegin(&scope); lockBoxes(owner);
    if (box->previous) box->previous->next=box->next; else owner->boxes=box->next;
    if (box->next) box->next->previous=box->previous;
    box->next=box->previous=NULL;
    unlockBoxes(owner); mutationEnd(&scope);
}
static void dropBox(ChannelOwner* owner,ChannelBox* box)
{
    if (!box) return;
    xrtTypeDropValue(xl6SequenceStorageTypeV1(owner->layout.transfer,owner->layout.type),box->slot);
    xrtFree(box);
}
/* Quiescent cleanup, including graph retirement. Embedding the channel avoids
 * losing a separately allocated wrapper when Unit reports a resource failure.
 * XRT Unit already attempts every cond/mutex duty. Independently owned boxes
 * are released even when that checked duty fails; no generated destructor is
 * called and a later failure cannot replace the first one. */
static bool dispose(ChannelOwner* owner)
{
    xl6_error_scope cleanup={0}; if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) abort();
    xl6_error_scope work={0}; if (!xl6ErrorScopeBegin(&work)) abort();
    if (owner->initialized) {
        xrtChannelClose(&owner->channel);
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&work);
        if (!xrtChannelUnit(&owner->channel) || xrtGetError()) xl6ErrorScopeCaptureFailure(&work);
        owner->initialized=false;
    }
    ChannelBox* box=owner->boxes; owner->boxes=NULL;
    while (box) {
        ChannelBox* next=box->next; dropBox(owner,box); box=next;
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&work);
    }
    xl6_error_result completed=xl6ErrorScopeEnd(&work); bool ok=completed.ok;
    if (!ok) xl6ErrorScopePropagateFailure(completed);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return ok;
}
static void drop(void* data,void* context)
{
    if (context) abort();
    ChannelOwner* owner=data; if (!owner) return;
    xrtownershipscope scope={0}; mutationBegin(&scope);
    int32_t left=xrtRefRelease(&owner->refs); if (left<0) abort();
    if (left) { mutationEnd(&scope); return; }
    if (xrtAtomic32Load(&owner->activity,XMEMORY_ACQUIRE) || owner->retired) abort();
    owner->cleared=true; mutationEnd(&scope);
    xl6_error_scope cleanup={0}; if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) abort();
    if (!dispose(owner)) xl6ErrorScopeCaptureFailure(&cleanup);
    xl6SequenceAdmissionDropV3(&owner->admission); xrtFree(owner);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
}
static bool retain(void* data,void** target,void* context)
{
    if (context) abort();
    ChannelOwner* owner=data;
    xrtownershipscope scope={0}; mutationBegin(&scope);
    bool ok=owner && target && !owner->cleared && xrtRefRetain(&owner->refs)>0;
    if (ok) *target=owner; else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&scope); return ok;
}
static const xvaluehandleops handleOps={retain,drop,NULL,NULL};
static bool countReferences(const void* data,size_t* count)
{
    const ChannelOwner* owner=data;
    if (!owner || !count || owner->cleared || owner->refs<=0 ||
        xrtAtomic32Load(&owner->activity,XMEMORY_ACQUIRE)) return false;
    *count=(size_t)owner->refs; return true;
}
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const ChannelOwner* owner=data; size_t count;
    if (!visit || !countReferences(owner,&count) || !owner->initialized) return false;
    if (owner->layout.transfer!=3) return true;
    /* Activity refusal includes pending sends, obtained receives, copies and
     * cleanup. At quiescence this resident intrusive ownership list is exact;
     * it does not peek into XRT's opaque ring or infer edges from Count(). */
    for (const ChannelBox* box=owner->boxes;box;box=box->next)
        if (!visit(xrtValueOwnership(*(xvalue* const*)box->slot),context)) return false;
    return true;
}
static const xrtownershipops ownerOps={countReferences,trace};
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    return visit && xrtValueGetHandle(value,&raw,&ops,&user) && raw && !user && ops==&handleOps &&
        visit((xrtownershipref){raw,&ownerOps},context);
}
static bool hold(const void* data)
{ ChannelOwner* owner=(ChannelOwner*)data; return owner->refs>0 && owner->refs<INT32_MAX && xrtRefRetain(&owner->refs)>0; }
static void releaseHold(const void* data) { drop((void*)data,NULL); }
static bool claim(const void* data,const void* token)
{
    ChannelOwner* owner=(ChannelOwner*)data;
    if (!token || owner->cleared || (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void restore(const void* data,const void* token)
{ ChannelOwner* owner=(ChannelOwner*)data; if (!token || owner->claim!=token || owner->cleared) abort(); owner->claim=NULL; }
static void clear(const void* data,const void* token)
{
    ChannelOwner* owner=(ChannelOwner*)data;
    if (!token || owner->claim!=token || owner->cleared || owner->retired ||
        xrtAtomic32Load(&owner->activity,XMEMORY_ACQUIRE)) abort();
    owner->cleared=true; owner->retired=true;
}
static bool finishClear(const void* data,const void* token)
{
    ChannelOwner* owner=(ChannelOwner*)data;
    if (!token || owner->claim!=token || !owner->cleared) abort();
    /* A checked physical failure is still reported on its first attempt.
     * XRT Unit and each box duty have already been attempted/consumed; the
     * retirement ledger may retry without invoking those destructors twice. */
    bool ok=dispose(owner); owner->retired=false; return ok;
}
const xrtownershipadapterv1* xl6ChannelOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,releaseHold,claim,restore,NULL,clear,finishClear};
    if (ref.Ops==&ownerOps && ref.Data) {
        size_t count; return countReferences(ref.Data,&count) ? &adapter : NULL;
    }
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}
static ChannelOwner* borrow(const xvalue* value,uint64_t identity)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    if (!identity || !value || xrtValueTypeId(value)!=identity ||
        !xrtValueGetHandle(value,&raw,&ops,&user) || ops!=&handleOps || !raw || user) {
        xrtSetErrorKind(XERR_TYPE); return NULL;
    }
    ChannelOwner* owner=raw;
    if (owner->cleared || !owner->initialized || owner->layout.identity!=identity) {
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    return owner;
}
static ChannelOwner* begin(const xvalue* value,uint64_t identity)
{
    xrtownershipscope scope={0}; mutationBegin(&scope);
    ChannelOwner* owner=borrow(value,identity);
    if (owner) {
        uint32_t active=xrtAtomic32Load(&owner->activity,XMEMORY_ACQUIRE);
        while (active<UINT32_MAX) {
            if (xrtAtomic32CompareExchange(&owner->activity,&active,active+1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) break;
        }
        if (active==UINT32_MAX) { xrtSetErrorKind(XERR_STATE); owner=NULL; }
        else if (xrtRefRetain(&owner->refs)<=0) abort();
    }
    mutationEnd(&scope); return owner;
}
static void end(ChannelOwner* owner)
{
    xrtownershipscope scope={0}; mutationBegin(&scope);
    if (!xrtAtomic32FetchSub(&owner->activity,1,XMEMORY_RELEASE)) abort();
    mutationEnd(&scope); drop(owner,NULL);
}
bool xl6ChannelAdmitsSchemaV1(const xvalue* value,uint64_t identity,
    const XL6_SequenceLayoutV1* layout,const XL6_SequenceAdmissionV3* policy)
{
    if (!value || !identity || !layout || !layout->identity || !policy || !policy->ops || !policy->data ||
        xrtValueType(value)!=XVALUE_HANDLE || xrtValueTypeId(value)!=identity) return false;
    xrtownershipscope scope={0}; mutationBegin(&scope);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL; bool admitted=false;
    if (xrtValueGetHandle(value,&raw,&ops,&user) && raw && !user && ops==&handleOps) {
        const ChannelOwner* owner=raw; const XL6_SequenceAdmissionV3* domain=&owner->admission;
        admitted=!owner->cleared && owner->layout.identity==identity && owner->schemaIdentity==layout->identity &&
            owner->layout.elementIdentity==layout->elementIdentity && owner->layout.transfer==layout->transfer &&
            owner->layout.size==layout->size && owner->layout.type==layout->type && domain->ops==policy->ops &&
            domain->elementIdentity==policy->elementIdentity && domain->transfer==policy->transfer &&
            domain->size==policy->size && domain->type==policy->type && domain->boxedIdentity==policy->boxedIdentity &&
            (domain->data==policy->data || domain->ops->Equivalent(domain->data,policy->data));
    }
    mutationEnd(&scope); return admitted;
}
xvalue* xl6ChannelCreateTakeV1(uint64_t identity,xvalue* schema,int64_t capacity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    ChannelOwner* owner=NULL; xvalue* result=NULL; XL6_SequenceRoleV1 role;
    if (!identity || capacity<0 || (uint64_t)capacity>SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); goto fail; }
    owner=xrtCalloc(1,sizeof(*owner)); if (!owner) goto fail;
    owner->refs=1; xrtAtomic32Init(&owner->activity,0);
    if (!xrtSpinInit(&owner->boxesLock) || !xl6SequenceSchemaCopyV1(schema,&role,&owner->layout,&owner->admission)) goto fail;
    if (owner->layout.transfer<1 || owner->layout.transfer>3) { xrtSetErrorKind(XERR_UNSUPPORTED); goto fail; }
    owner->schemaIdentity=owner->layout.identity; owner->layout.identity=identity;
    if (!xrtChannelInit(&owner->channel,(size_t)capacity)) goto fail;
    owner->initialized=true;
    result=xrtValueHandleTake((void**)&owner,&handleOps,NULL);
    if (!result || !xrtValueTypeIdBind(result,identity) || !xrtValueHandleOwnershipBindPhased(result,handleTrace)) goto fail;
    goto done;
fail:
    xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    if (owner) drop(owner,NULL);
done:
    xrtValueRelease(schema);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xrtValueRelease(result); xl6ErrorScopePropagateFailure(outcome); return NULL; }
    return result;
}
static bool overlaps(const void* a,size_t as,const void* b,size_t bs)
{ return (uintptr_t)a<(uintptr_t)b+bs && (uintptr_t)b<(uintptr_t)a+as; }
static bool request(ChannelOwner* owner,uint64_t element,const xrttype* type,XL6_ChannelSlotV1 slot,
    XL6_ChannelWaitV1 mode,int64_t milliseconds,XL6_ChannelStatusV1* status,bool receive)
{
    if (owner->layout.elementIdentity!=element || owner->layout.type!=type || owner->layout.size!=slot.size) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    if (!slot.data || !status || !type->Align || (uintptr_t)slot.data%type->Align ||
        (uintptr_t)slot.data>UINTPTR_MAX-slot.size || (uintptr_t)status%_Alignof(XL6_ChannelStatusV1) ||
        (uintptr_t)status>UINTPTR_MAX-sizeof(*status) || overlaps(slot.data,slot.size,status,sizeof(*status)) ||
        overlaps(slot.data,slot.size,owner,sizeof(*owner)) || overlaps(status,sizeof(*status),owner,sizeof(*owner)) ||
        mode<XL6_CHANNEL_TRY_V1 || mode>XL6_CHANNEL_FOR_V1 || milliseconds<0 ||
        (mode!=XL6_CHANNEL_FOR_V1 && milliseconds)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (receive) {
        bool empty=owner->layout.transfer==3 ? *(xvalue* const*)slot.data==NULL :
            owner->layout.transfer==2 ? ((const xstrview*)slot.data)->Data==NULL && ((const xstrview*)slot.data)->Size==0 : true;
        if (!empty) { xrtSetErrorKind(XERR_STATE); return false; }
    } else if (!owner->admission.ops->Slot(owner->admission.data,slot.data)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    return true;
}
static bool statusFromTry(xchannelresult result,XL6_ChannelStatusV1* status)
{
    switch (result) {
    case XCHANNEL_OK: *status=XL6_CHANNEL_OK_V1; return true;
    case XCHANNEL_EMPTY: *status=XL6_CHANNEL_EMPTY_V1; return true;
    case XCHANNEL_FULL: *status=XL6_CHANNEL_FULL_V1; return true;
    case XCHANNEL_CLOSED: *status=XL6_CHANNEL_CLOSED_V1; return true;
    default: return false;
    }
}
static bool statusFromWait(xwaitresult result,XL6_ChannelStatusV1* status)
{
    switch (result) {
    case XWAIT_OK: *status=XL6_CHANNEL_OK_V1; return true;
    case XWAIT_TIMEOUT: *status=XL6_CHANNEL_TIMEOUT_V1; return true;
    case XWAIT_CLOSED: *status=XL6_CHANNEL_CLOSED_V1; return true;
    default: return false;
    }
}
static ChannelBox* copyBox(ChannelOwner* owner,const void* item)
{
    const xrttype* type=xl6SequenceStorageTypeV1(owner->layout.transfer,owner->layout.type);
    if (!type->Align || type->Align>_Alignof(max_align_t) || (type->Align&(type->Align-1))) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    size_t offset=(sizeof(ChannelBox)+type->Align-1)&~(type->Align-1);
    if (offset>SIZE_MAX-type->Size) { xrtSetErrorKind(XERR_RANGE); return NULL; }
    ChannelBox* box=xrtCalloc(1,offset+type->Size); if (!box) return NULL;
    box->slot=(unsigned char*)box+offset;
    if (!xrtTypeCopyValue(type,box->slot,item)) {
        xl6_error_scope cleanup={0}; if (!xl6ErrorScopeBeginCleanup(&cleanup,true)) abort();
        dropBox(owner,box);
        xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
        return NULL;
    }
    return box;
}
bool xl6ChannelOperationOpenV1(const xvalue* value,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,bool sending,XL6_ChannelStatusV1* status,XL6_ChannelOperationV1* output)
{
    if (!output || output->owner || output->native || output->message || output->received ||
        output->target.data || output->target.size || output->sending) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    ChannelOwner* owner=begin(value,identity);
    if (!owner) return false;
    if (!request(owner,element,type,slot,XL6_CHANNEL_TRY_V1,0,status,!sending)) { end(owner); return false; }
    ChannelBox* box=sending ? copyBox(owner,slot.data) : NULL;
    if (sending && !box) { end(owner); return false; }
    if (box) linkBox(owner,box);
    *output=(XL6_ChannelOperationV1){.owner=owner,.native=&owner->channel,.message=box,
        .target=sending ? (XL6_ChannelSlotV1){0} : slot,.sending=sending};
    return true;
}
bool xl6ChannelOperationCommitV1(XL6_ChannelOperationV1* operation)
{
    if (!operation || !operation->owner) { xrtSetErrorKind(XERR_STATE); return false; }
    ChannelOwner* owner=operation->owner;
    if (operation->sending) {
        /* A concurrent receiver may already have freed this box. */
        if (!operation->message) { xrtSetErrorKind(XERR_STATE); return false; }
        operation->message=NULL; return true;
    }
    ChannelBox* box=operation->received;
    if (!box) { xrtSetErrorKind(XERR_STATE); return false; }
    operation->received=NULL; unlinkBox(owner,box);
    memcpy(operation->target.data,box->slot,operation->target.size);
    memset(box->slot,0,operation->target.size); xrtFree(box); return true;
}
void xl6ChannelOperationCloseV1(XL6_ChannelOperationV1* operation)
{
    if (!operation) return;
    ChannelOwner* owner=operation->owner;
    ChannelBox* unsent=operation->message; ChannelBox* acquired=operation->received;
    memset(operation,0,sizeof(*operation));
    if (!owner) return;
    if (unsent) { unlinkBox(owner,unsent); dropBox(owner,unsent); }
    if (acquired) { unlinkBox(owner,acquired); dropBox(owner,acquired); }
    end(owner);
}
static bool legacyOperation(const xvalue* value,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitV1 mode,int64_t milliseconds,XL6_ChannelStatusV1* status,bool sending)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    XL6_ChannelOperationV1 operation={0}; bool ok=false;
    xdeadline deadline=XRT_DEADLINE_NEVER;
    XL6_ChannelStatusV1 actual=XL6_CHANNEL_OK_V1;
    bool valid=mode>=XL6_CHANNEL_TRY_V1 && mode<=XL6_CHANNEL_FOR_V1 && milliseconds>=0 &&
        (mode==XL6_CHANNEL_FOR_V1 || !milliseconds);
    if (!valid) xrtSetErrorKind(XERR_ARGUMENT);
    if (valid &&
        (mode!=XL6_CHANNEL_FOR_V1 || xl6ConcurrentDeadline(milliseconds,&deadline))) {
        if (xl6ChannelOperationOpenV1(value,identity,element,type,slot,sending,status,&operation)) {
            ok=mode==XL6_CHANNEL_TRY_V1 ? statusFromTry(sending ?
                xrtChannelTrySend(operation.native,operation.message) : xrtChannelTryRecv(operation.native,&operation.received),&actual) :
                statusFromWait(sending ? xrtChannelSendUntil(operation.native,operation.message,deadline) :
                    xrtChannelRecvUntil(operation.native,&operation.received,deadline),&actual);
            if (ok && actual==XL6_CHANNEL_OK_V1) ok=xl6ChannelOperationCommitV1(&operation);
        }
    }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6ChannelOperationCloseV1(&operation);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    *status=actual; return true;
}
bool xl6ChannelSendV1(const xvalue* value,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitV1 mode,int64_t milliseconds,XL6_ChannelStatusV1* status)
{ return legacyOperation(value,identity,element,type,slot,mode,milliseconds,status,true); }
bool xl6ChannelReceiveV1(const xvalue* value,uint64_t identity,uint64_t element,const xrttype* type,
    XL6_ChannelSlotV1 slot,XL6_ChannelWaitV1 mode,int64_t milliseconds,XL6_ChannelStatusV1* status)
{ return legacyOperation(value,identity,element,type,slot,mode,milliseconds,status,false); }
int64_t xl6ChannelQueryV1(const xvalue* value,uint64_t identity,unsigned query)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    ChannelOwner* owner=begin(value,identity); int64_t result=-1;
    if (owner) switch (query) {
    case XL6_CHANNEL_QUERY_COUNT_V1: result=(int64_t)xrtChannelCount(&owner->channel); break;
    case XL6_CHANNEL_QUERY_CAPACITY_V1: result=(int64_t)xrtChannelCapacity(&owner->channel); break;
    case XL6_CHANNEL_QUERY_CLOSED_V1: result=xrtChannelIsClosed(&owner->channel); break;
    case XL6_CHANNEL_QUERY_DRAINED_V1: result=xrtChannelIsDrained(&owner->channel); break;
    default: xrtSetErrorKind(XERR_ARGUMENT); break;
    }
    if (result<0 || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) end(owner);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return -1; }
    return result;
}
bool xl6ChannelCloseV1(const xvalue* value,uint64_t identity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    ChannelOwner* owner=begin(value,identity);
    if (owner) xrtChannelClose(&owner->channel);
    if (!owner || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) end(owner);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    return true;
}
