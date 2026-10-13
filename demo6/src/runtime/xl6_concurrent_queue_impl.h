/* Intentionally instantiated in independent resident TUs. The macro
 * backend selects a direct typed XRT implementation at compile time, not a
 * runtime kind switch/function table that pulls every queue into an artifact. */
#include "xl6_concurrent_queue.h"
#include "xl6_array_reference_value.h"
#include "src/api/xl_foreign_error.h"
#if defined(XL6_Q_WAIT)
#include "xl6_cond_internal.h"
#endif
#include <limits.h>
#include <stdlib.h>
#define XL6_Q_JOIN_(A,B) A##B
#define XL6_Q_JOIN(A,B) XL6_Q_JOIN_(A,B)
#define XL6_Q_PUBLIC(Suffix) XL6_Q_JOIN(XL6_Q_PREFIX,Suffix)
#define XL6_Q_NATIVE(Suffix) XL6_Q_JOIN(XL6_Q_BACKEND,Suffix)
typedef struct QueueOwner {
    volatile int32_t refs;
    XL6_Q_TYPE* queue;
    XL6_Q_TYPE* retired;
    XL6_SequenceLayoutV1 layout;
    uint64_t schemaIdentity;
    XL6_SequenceAdmissionV3 admission;
    xatomic32 activity, producer, consumer;
    const void* claim;
    bool cleared;
#if defined(XL6_Q_WAIT)
    xvalue* waitMutex;
    xvalue* available;
#endif
} QueueOwner;
enum { EXCLUSIVE=UINT32_MAX };
static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static void destroy(XL6_Q_TYPE* queue)
{
    XL6_Q_NATIVE(Destroy)(queue);
}
#if defined(XL6_Q_WAIT)
static void waitUnit(QueueOwner* owner,xl6_error_scope* scope)
{
    /* Resident Sync owners keep failed physical retirement in the existing
     * checked host ledger. No lost raw allocation or automatic forced unlock.
     * Clear fields before release so graph retries never repeat a duty. */
    xvalue* available=owner->available;owner->available=NULL;xrtValueRelease(available);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(scope);
    xvalue* mutex=owner->waitMutex;owner->waitMutex=NULL;xrtValueRelease(mutex);
    if(xrtGetError())xl6ErrorScopeCaptureFailure(scope);
}
#endif
static void drop(void* data, void* context)
{
    if (context) abort();
    QueueOwner* owner=data;
    if (!owner) return;
    xrtownershipscope scope={0}; mutationBegin(&scope);
    int32_t left=xrtRefRelease(&owner->refs); if (left<0) abort();
    if (left) { mutationEnd(&scope); return; }
    if (xrtAtomic32Load(&owner->activity,XMEMORY_ACQUIRE) || owner->retired) abort();
    XL6_Q_TYPE* queue=owner->queue; owner->queue=NULL;
    mutationEnd(&scope);
    xl6_error_scope cleanup={0}; if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) abort();
    destroy(queue);
#if defined(XL6_Q_WAIT)
    waitUnit(owner,&cleanup);
#endif
    xl6SequenceAdmissionDropV3(&owner->admission); xrtFree(owner);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
}
static bool retain(void* data, void** result, void* context)
{
    if (context) abort();
    QueueOwner* owner=data;
    xrtownershipscope scope={0}; mutationBegin(&scope);
    bool ok=owner && result && !owner->cleared && xrtRefRetain(&owner->refs)>0;
    if (ok) *result=owner; else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&scope); return ok;
}
static const xvaluehandleops handleOps={retain,drop,NULL,NULL};
/* Probe immutable admission metadata, including the genuine provider and the
 * canonical source-schema identity. Activity/BUSY is allowed: no queue storage
 * is touched and no producer/consumer lane is entered. Refuse foreign handles
 * before dereferencing their raw pointer. The world gate protects retirement. */
bool XL6_Q_PUBLIC(AdmitsSchemaV1)(const xvalue* value,uint64_t identity,
    const XL6_SequenceLayoutV1* layout,const XL6_SequenceAdmissionV3* policy)
{
    if (!value || !identity || !layout || !policy || !policy->ops ||
        xrtValueType(value)!=XVALUE_HANDLE || xrtValueTypeId(value)!=identity) return false;
    xrtownershipscope scope={0}; mutationBegin(&scope);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    bool admitted=false;
    if (xrtValueGetHandle(value,&raw,&ops,&user) && ops==&handleOps && raw && !user) {
        const QueueOwner* owner=raw;
        const XL6_SequenceLayoutV1* actual=&owner->layout;
        const XL6_SequenceAdmissionV3* domain=&owner->admission;
        admitted=!owner->cleared && actual->identity==identity &&
            owner->schemaIdentity==layout->identity &&
            actual->elementIdentity==layout->elementIdentity && actual->transfer==layout->transfer &&
            actual->size==layout->size && actual->type==layout->type && domain->ops==policy->ops &&
            domain->elementIdentity==policy->elementIdentity && domain->transfer==policy->transfer &&
            domain->size==policy->size && domain->type==policy->type &&
            domain->boxedIdentity==policy->boxedIdentity &&
            (domain->data==policy->data || domain->ops->Equivalent(domain->data,policy->data));
    }
    mutationEnd(&scope); return admitted;
}
static bool countReferences(const void* data, size_t* count)
{
    const QueueOwner* owner=data;
    if (!owner || !count || owner->cleared || owner->refs<=0 ||
        xrtAtomic32Load(&owner->activity,XMEMORY_ACQUIRE)) return false;
    *count=(size_t)owner->refs; return true;
}
static bool trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const QueueOwner* owner=data; size_t count;
    if (!visit || !countReferences(owner,&count) || !owner->queue) return false;
#if defined(XL6_Q_WAIT)
    if(!visit(xrtValueOwnership(owner->waitMutex),context) ||
        !visit(xrtValueOwnership(owner->available),context))return false;
#endif
    if (owner->layout.transfer!=3 && owner->layout.transfer!=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) return true;
    const xtypedqueuecore* core=&owner->queue->Core;
    /* Graph freeze excludes new participating operations. Activity pins cause
     * refusal, never waiting while arbitrary value callbacks are still running.
     * Every fixed slot is initialized; empty/retry/in-flight ownership cannot be
     * missed by peeking only at a ready ring or its approximate Count. */
    for (size_t i=0;i<core->Capacity;++i) {
        const void* slot=core->Values+i*core->Stride;
        if (owner->layout.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) {
            if (!owner->admission.ops->TraceSlot ||
                !owner->admission.ops->TraceSlot(owner->admission.data,slot,visit,context)) return false;
        } else if (!visit(xrtValueOwnership(*(xvalue* const*)slot),context)) return false;
    }
    return true;
}
static const xrtownershipops ownerOps={countReferences,trace};
static bool handleTrace(const xvalue* value, xrtownershipvisitor visit, void* context)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL;
    return visit && xrtValueGetHandle(value,&raw,&ops,&user) && raw && !user && ops==&handleOps &&
        visit((xrtownershipref){raw,&ownerOps},context);
}
static bool hold(const void* data)
{ QueueOwner* owner=(QueueOwner*)data; return owner->refs>0 && owner->refs<INT32_MAX && xrtRefRetain(&owner->refs)>0; }
static void releaseHold(const void* data) { drop((void*)data,NULL); }
static bool claim(const void* data, const void* token)
{
    QueueOwner* owner=(QueueOwner*)data;
    if (!token || owner->cleared || (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void restore(const void* data, const void* token)
{ QueueOwner* owner=(QueueOwner*)data; if (!token || owner->claim!=token || owner->cleared) abort(); owner->claim=NULL; }
static void clear(const void* data, const void* token)
{
    QueueOwner* owner=(QueueOwner*)data;
    if (!token || owner->claim!=token || owner->cleared || owner->retired ||
        xrtAtomic32Load(&owner->activity,XMEMORY_ACQUIRE)) abort();
    owner->retired=owner->queue; owner->queue=NULL; owner->cleared=true;
}
static bool finishClear(const void* data, const void* token)
{
    QueueOwner* owner=(QueueOwner*)data;
    xrtownershipscope scope={0}; mutationBegin(&scope);
    if (!token || owner->claim!=token || !owner->cleared) abort();
    XL6_Q_TYPE* retired=owner->retired; owner->retired=NULL;
    mutationEnd(&scope);
    xl6_error_scope cleanup={0}; if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) abort();
    /* Preserve the caller's primary failure separately from this duty's
     * completion. An inherited failure must not turn an actually successful
     * physical release into an endlessly retrying Finish=false. */
    xl6_error_scope work={0}; if (!xl6ErrorScopeBegin(&work)) abort();
    destroy(retired);
#if defined(XL6_Q_WAIT)
    waitUnit(owner,&work);
#endif
    xl6_error_result completed=xl6ErrorScopeEnd(&work); bool ok=completed.ok;
    if (!ok) xl6ErrorScopePropagateFailure(completed);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return ok;
}
const xrtownershipadapterv1* XL6_Q_PUBLIC(OwnershipAdapterV1)(xrtownershipref ref,
    const XL6_SequenceAdmissionOpsV2* const* policies, size_t count)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,releaseHold,claim,restore,NULL,clear,finishClear};
    if (count && !policies) return NULL;
    if (ref.Ops==&ownerOps && ref.Data) {
        const QueueOwner* owner=ref.Data; size_t references;
        if (owner->layout.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) {
            bool admitted=false;
            for (size_t i=0;i<count;++i) if (policies[i]==owner->admission.ops) { admitted=true; break; }
            if (!admitted) return NULL;
        }
        return countReferences(owner,&references) ? &adapter : NULL;
    }
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}
static QueueOwner* begin(const xvalue* value, uint64_t identity, unsigned lane)
{
    xrtownershipscope scope={0}; mutationBegin(&scope);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* ops=NULL; QueueOwner* owner=NULL;
    if (!identity || !value || xrtValueTypeId(value)!=identity ||
        !xrtValueGetHandle(value,&raw,&ops,&user) || ops!=&handleOps || !raw || user) xrtSetErrorKind(XERR_TYPE);
    else {
        QueueOwner* candidate=raw;
        uint32_t active=xrtAtomic32Load(&candidate->activity,XMEMORY_ACQUIRE);
        bool entered=false;
        if (!candidate->cleared && candidate->queue && candidate->layout.identity==identity) {
            while (active<UINT32_MAX-1u && (lane!=3 || !active)) {
                if (xrtAtomic32CompareExchange(&candidate->activity,&active,
                    lane==3 ? EXCLUSIVE : active+1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) { entered=true; break; }
            }
        }
        if (!entered) xrtSetErrorKind(XERR_STATE);
        else {
            xatomic32* role=lane==1 && XL6_Q_SINGLE_PRODUCER ? &candidate->producer :
                lane==2 && XL6_Q_SINGLE_CONSUMER ? &candidate->consumer : NULL;
            uint32_t expected=0;
            if (role && !xrtAtomic32CompareExchange(role,&expected,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
                (void)xrtAtomic32FetchSub(&candidate->activity,1,XMEMORY_RELEASE);
                xrtSetErrorKind(XERR_STATE);
            }
            else if (xrtRefRetain(&candidate->refs)<=0) abort();
            else owner=candidate;
        }
    }
    mutationEnd(&scope); return owner;
}
static void end(QueueOwner* owner, unsigned lane)
{
    xrtownershipscope scope={0}; mutationBegin(&scope);
    uint32_t active=xrtAtomic32Load(&owner->activity,XMEMORY_ACQUIRE);
    if (!active || (lane==3)!=(active==EXCLUSIVE)) abort();
    if (lane==1 && XL6_Q_SINGLE_PRODUCER) xrtAtomic32Store(&owner->producer,0,XMEMORY_RELEASE);
    if (lane==2 && XL6_Q_SINGLE_CONSUMER) xrtAtomic32Store(&owner->consumer,0,XMEMORY_RELEASE);
    if (lane==3) xrtAtomic32Store(&owner->activity,0,XMEMORY_RELEASE);
    else (void)xrtAtomic32FetchSub(&owner->activity,1,XMEMORY_RELEASE);
    mutationEnd(&scope); drop(owner,NULL);
}
static bool tuple(QueueOwner* owner, uint64_t element, size_t size, const xrttype* type)
{
    if (owner->layout.elementIdentity==element && owner->layout.size==size && owner->layout.type==type) return true;
    xrtSetErrorKind(XERR_TYPE); return false;
}
static bool range(const void* items, size_t count, size_t size, const xrttype* type)
{
    if (!count) return true;
    if (!items || !size || count>SIZE_MAX/size || (uintptr_t)items%type->Align ||
        count*size>UINTPTR_MAX-(uintptr_t)items) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return true;
}
xvalue* XL6_Q_PUBLIC(CreateTakeV1)(uint64_t identity, xvalue* schema, int64_t capacity)
{
    xl6_error_scope transaction={0}; if (!xl6ErrorScopeBegin(&transaction)) abort();
    QueueOwner* owner=NULL; xvalue* result=NULL; XL6_SequenceRoleV1 role;
    if (!identity || capacity<=0 || (uint64_t)capacity>XRT_QUEUE_MAX_CAPACITY) { xrtSetErrorKind(XERR_RANGE); goto fail; }
    owner=xrtCalloc(1,sizeof(*owner)); if (!owner) goto fail;
    owner->refs=1;
    xrtAtomic32Init(&owner->activity,0); xrtAtomic32Init(&owner->producer,0); xrtAtomic32Init(&owner->consumer,0);
    if (!xl6SequenceSchemaCopyV1(schema,&role,&owner->layout,&owner->admission)) goto fail;
    owner->schemaIdentity=owner->layout.identity;
    owner->layout.identity=identity;
#if defined(XL6_Q_WAIT)
    owner->waitMutex=xl6MutexCreateV1(identity);if(!owner->waitMutex)goto fail;
    owner->available=xl6CondCreateV1(identity);if(!owner->available)goto fail;
#endif
    owner->queue=XL6_Q_NATIVE(Create)(xl6SequenceStorageTypeV1(owner->layout.transfer,owner->layout.type),(size_t)capacity);
    if (!owner->queue) goto fail;
    if (XL6_Q_NATIVE(Capacity)(owner->queue)>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); goto fail; }
    result=xrtValueHandleTake((void**)&owner,&handleOps,NULL);
    if (!result || !xrtValueTypeIdBind(result,identity) || !xrtValueHandleOwnershipBindPhased(result,handleTrace)) goto fail;
    goto done;
fail:
    xl6ErrorScopeCaptureFailure(&transaction); xrtValueRelease(result); result=NULL;
    if (owner) drop(owner,NULL);
done:
    xrtValueRelease(schema);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&transaction);
    xl6_error_result outcome=xl6ErrorScopeEnd(&transaction);
    if (!outcome.ok) { xrtValueRelease(result); xl6ErrorScopePropagateFailure(outcome); return NULL; }
    return result;
}
xqueueresult XL6_Q_PUBLIC(PushV1)(const xvalue* value, uint64_t identity, uint64_t element,
    size_t size, const xrttype* type, const void* item)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    QueueOwner* owner=begin(value,identity,1); xqueueresult result=XQUEUE_ERROR;
    if (owner && tuple(owner,element,size,type)) {
        if (!range(item,1,size,type)) { /* Refuse before a typed predicate reads. */ }
        else if (!owner->admission.ops->Slot(owner->admission.data,item)) xrtSetErrorKind(XERR_TYPE);
        else {
#if defined(XL6_Q_WAIT)
            if(xl6MutexLockV1(owner->waitMutex,identity)) {
                result=XL6_Q_NATIVE(TryPush)(owner->queue,item);
                if(result==XQUEUE_OK && !xl6CondSignalV1(owner->available,identity))xl6ErrorScopeCaptureFailure(&scope);
                if(!xl6MutexUnlockV1(owner->waitMutex,identity))xl6ErrorScopeCaptureFailure(&scope);
            }
#else
            result=XL6_Q_NATIVE(TryPush)(owner->queue,item);
#endif
        }
    }
    if (result==XQUEUE_ERROR || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) end(owner,1);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return XQUEUE_ERROR; }
    return result;
}
#if !defined(XL6_Q_WAIT)
/* Caller owns the lane pin and error scope. This is the sole resident prefix
 * loop, shared by physical-slot and native-Array transports. No nested lane
 * entry (which would reject SPSC) and no generated wire conversion loop. */
static xqueuebatchresult transport(QueueOwner* owner, void* items, size_t count, bool pop)
{
    xqueuebatchresult result={XQUEUE_ERROR,0};
    size_t size=owner->layout.size;
    if (range(items,count,size,owner->layout.type)) {
        bool admitted=true;
        if (!pop) for (size_t i=0;i<count;++i)
            if (!owner->admission.ops->Slot(owner->admission.data,(const unsigned char*)items+i*size)) {
                admitted=false; xrtSetErrorKind(XERR_TYPE); break;
            }
        if (admitted) {
            result.Result=XQUEUE_OK;
            while (result.Count<count) {
                void* item=(unsigned char*)items+result.Count*size;
                xqueueresult step=pop ? XL6_Q_NATIVE(TryPop)(owner->queue,item) : XL6_Q_NATIVE(TryPush)(owner->queue,item);
                if (step!=XQUEUE_OK) {
                    /* XRT batch's prefix OK alone loses ERROR without a
                     * diagnostic. Keep the exact failing step independently,
                     * plus the successfully published/moved prefix count. */
                    result.Result=step==XQUEUE_ERROR || !result.Count ? step : XQUEUE_OK;
                    break;
                }
                ++result.Count;
            }
        }
    }
    return result;
}
static xqueuebatchresult batch(const xvalue* value, uint64_t identity, uint64_t element,
    size_t size, const xrttype* type, void* items, size_t count, bool pop)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    unsigned lane=pop ? 2 : 1;
    QueueOwner* owner=begin(value,identity,lane);
    xqueuebatchresult result={XQUEUE_ERROR,0};
    if (owner && tuple(owner,element,size,type)) result=transport(owner,items,count,pop);
    if (result.Result==XQUEUE_ERROR || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) end(owner,lane);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); result.Result=XQUEUE_ERROR; }
    return result;
}
xqueuebatchresult XL6_Q_PUBLIC(PushBatchV1)(const xvalue* value,uint64_t identity,uint64_t element,
    size_t size,const xrttype* type,const void* items,size_t count)
{ return batch(value,identity,element,size,type,(void*)items,count,false); }
xqueuebatchresult XL6_Q_PUBLIC(PopBatchV1)(const xvalue* value,uint64_t identity,uint64_t element,
    size_t size,const xrttype* type,void* items,size_t count)
{ return batch(value,identity,element,size,type,items,count,true); }
xqueuebatchresult XL6_Q_PUBLIC(PushArrayV2)(const xvalue* value,uint64_t identity,
    const xvalue* array,uint64_t arrayIdentity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    QueueOwner* owner=begin(value,identity,1);
    XL6_Sequence* source=NULL;
    xqueuebatchresult result={XQUEUE_ERROR,0};
    if (owner) {
        XL6_SequenceLayoutV1 layout=owner->layout; layout.identity=arrayIdentity;
        /* Exact logical policy, not just tag/width. Role/ops are checked before
         * opaque handles are dereferenced; the pin protects storage afterward. */
        if (!xl6SequenceAdmitsSchemaV1(array,XL6_SEQUENCE_ARRAY_V1,&layout,&owner->admission))
            xrtSetErrorKind(XERR_TYPE);
        else source=xl6SequenceBeginRoleV1(array,XL6_SEQUENCE_ARRAY_V1,arrayIdentity,
            layout.elementIdentity,layout.transfer,layout.size,layout.type);
        if (source) {
            xtypedarray* storage=xl6SequenceStorageV1(source);
            result=transport(owner,(void*)xrtTypedArrayConstData(storage),xrtTypedArrayCount(storage),false);
        }
    }
    if (result.Result==XQUEUE_ERROR || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (source) xl6SequenceEndV1(source);
    if (owner) end(owner,1);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); result.Result=XQUEUE_ERROR; }
    return result;
}
XL6_QueueArrayResultV2 XL6_Q_PUBLIC(PopArrayV2)(const xvalue* value,uint64_t identity,
    uint64_t arrayIdentity,int64_t maximum,const xvalueconverter* projection)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    XL6_QueueArrayResultV2 result={XQUEUE_ERROR,0,NULL};
    QueueOwner* owner=NULL; XL6_Sequence* target=NULL;
    if (!arrayIdentity || maximum<0 || (uint64_t)maximum>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); goto done;
    }
    owner=begin(value,identity,2); if (!owner) goto done;
    if (owner->layout.transfer==3) {
        if (projection) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
        projection=xl6ArrayReferenceConverterV1();
    }
    /* Own only immutable resident metadata. No source witness/storage or
     * generated converter callback is kept. The IR supplies the canonical
     * output Array identity and immutable resident projection expression. */
    if (!owner->admission.ops->Retain(owner->admission.data)) goto done;
    XL6_SequenceAdmissionV3 admission=owner->admission;
    result.Items=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_ARRAY_V1,arrayIdentity,&admission);
    if (!result.Items) goto done;
    target=xl6SequenceBeginRoleV1(result.Items,XL6_SEQUENCE_ARRAY_V1,arrayIdentity,
        owner->layout.elementIdentity,owner->layout.transfer,owner->layout.size,owner->layout.type);
    if (!target || !xl6SequenceBindProjectionV1(target,projection)) goto prepareFailure;
    size_t capacity=XL6_Q_NATIVE(Capacity)(owner->queue);
    size_t limit=(size_t)maximum<capacity ? (size_t)maximum : capacity;
    xtypedarray* storage=xl6SequenceStorageV1(target);
    /* Native Init creates transfer destinations, not public language defaults.
     * The fresh owner is BUSY/unpublished throughout; graph readers refuse it.
     * All allocation/init occurs BEFORE a queue item can be consumed. */
    if (!xrtTypedArrayResize(storage,limit)) goto prepareFailure;
    xqueuebatchresult moved=transport(owner,xrtTypedArrayData(storage),limit,true);
    result.Result=moved.Result; result.Count=moved.Count;
    /* Shrinking a tail of empty transfer destinations cannot allocate or
     * invoke an owned user destructor. Keep only the successfully moved prefix.
     * Resident descriptors, never an arbitrary custom type, define this ABI. */
    if (!xrtTypedArrayResize(storage,moved.Count)) abort();
    goto done;
prepareFailure:
    if (target) { xl6SequenceEndV1(target); target=NULL; }
    xrtValueRelease(result.Items); result.Items=NULL;
done:
    if (result.Result==XQUEUE_ERROR || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (target) xl6SequenceEndV1(target);
    if (owner) end(owner,2);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); result.Result=XQUEUE_ERROR; }
    return result;
}
xqueueresult XL6_Q_PUBLIC(PopV1)(const xvalue* value, uint64_t identity, uint64_t element,
    size_t size, const xrttype* type, void* target)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    QueueOwner* owner=begin(value,identity,2); xqueueresult result=XQUEUE_ERROR;
    if (owner && tuple(owner,element,size,type) && range(target,1,size,type))
        result=XL6_Q_NATIVE(TryPop)(owner->queue,target);
    if (result==XQUEUE_ERROR || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) end(owner,2);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return XQUEUE_ERROR; }
    return result;
}
#else
/* One absolute monotonic deadline survives all spurious wakeups. A timed
 * wake always rechecks the queue while holding the publication mutex: a
 * published item or close wins over a simultaneous timeout. */
bool XL6_Q_PUBLIC(ReceiveV1)(const xvalue* value,uint64_t identity,uint64_t element,
    const xrttype* type,XL6_ChannelSlotV1 slot,XL6_ChannelWaitV1 mode,
    int64_t milliseconds,XL6_ChannelStatusV1* status)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    QueueOwner* owner=begin(value,identity,2);bool ok=false,locked=false;
    XL6_ChannelStatusV1 actual=XL6_CHANNEL_EMPTY_V1;
    if(!owner)goto done;
    if(!tuple(owner,element,slot.size,type) || !range(slot.data,1,slot.size,type))goto done;
    uintptr_t output=(uintptr_t)slot.data,mark=(uintptr_t)status;
    if(!status || mark%_Alignof(XL6_ChannelStatusV1) || mark>UINTPTR_MAX-sizeof(*status) ||
        (output<mark+sizeof(*status) && mark<output+slot.size) ||
        (output<(uintptr_t)owner+sizeof(*owner) && (uintptr_t)owner<output+slot.size) ||
        (mark<(uintptr_t)owner+sizeof(*owner) && (uintptr_t)owner<mark+sizeof(*status)) ||
        mode<XL6_CHANNEL_TRY_V1 || mode>XL6_CHANNEL_FOR_V1 || milliseconds<0 ||
        (mode!=XL6_CHANNEL_FOR_V1 && milliseconds)) {xrtSetErrorKind(XERR_ARGUMENT);goto done;}
    if((owner->layout.transfer==3 && *(xvalue* const*)slot.data) ||
        (owner->layout.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1 && *(void* const*)slot.data) ||
        (owner->layout.transfer==2 && (((xstrview*)slot.data)->Data || ((xstrview*)slot.data)->Size))) {
        xrtSetErrorKind(XERR_STATE);goto done;
    }
    uint64_t micros=(uint64_t)milliseconds>UINT64_MAX/1000?UINT64_MAX:(uint64_t)milliseconds*1000;
    xdeadline deadline=mode==XL6_CHANNEL_FOR_V1?xrtDeadlineAfter(micros):0;
    if(!xl6MutexLockV1(owner->waitMutex,identity))goto done;
    locked=true;
    bool timedOut=false;
    for(;;) {
        xqueueresult result=XL6_Q_NATIVE(TryPop)(owner->queue,slot.data);
        if(result==XQUEUE_OK){actual=XL6_CHANNEL_OK_V1;ok=true;break;}
        if(result==XQUEUE_CLOSED){actual=XL6_CHANNEL_CLOSED_V1;ok=true;break;}
        if(result!=XQUEUE_EMPTY)break;
        if(mode==XL6_CHANNEL_TRY_V1){actual=XL6_CHANNEL_EMPTY_V1;ok=true;break;}
        if(timedOut || (mode==XL6_CHANNEL_FOR_V1 && xrtDeadlineExpired(deadline))) {
            actual=XL6_CHANNEL_TIMEOUT_V1;ok=true;break;
        }
        xwaitresult waited=xl6CondWaitUntilNative(owner->available,identity,owner->waitMutex,identity,
            mode==XL6_CHANNEL_WAIT_V1?XRT_DEADLINE_NEVER:deadline);
        if(waited==XWAIT_TIMEOUT)timedOut=true;
        else if(waited!=XWAIT_OK)break;
    }
done:
    if(locked && !xl6MutexUnlockV1(owner->waitMutex,identity))xl6ErrorScopeCaptureFailure(&scope);
    if(!ok || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(owner)end(owner,2);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return false;}
    *status=actual;return true;
}
#endif
int64_t XL6_Q_PUBLIC(QueryV1)(const xvalue* value, uint64_t identity, unsigned query)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    QueueOwner* owner=begin(value,identity,0); int64_t result=-1;
    if (owner) switch (query) {
    case XL6_QUEUE_QUERY_COUNT_V1: result=(int64_t)XL6_Q_NATIVE(Count)(owner->queue); break;
    case XL6_QUEUE_QUERY_CAPACITY_V1: result=(int64_t)XL6_Q_NATIVE(Capacity)(owner->queue); break;
    case XL6_QUEUE_QUERY_CLOSED_V1: result=XL6_Q_NATIVE(IsClosed)(owner->queue); break;
    case XL6_QUEUE_QUERY_DRAINED_V1: result=XL6_Q_NATIVE(IsDrained)(owner->queue); break;
    default: xrtSetErrorKind(XERR_ARGUMENT); break;
    }
    if (result<0 || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) end(owner,0);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return -1; }
    return result;
}
#if !defined(XL6_Q_WAIT)
static bool transition(const xvalue* value, uint64_t identity, bool reset)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) abort();
    QueueOwner* owner=begin(value,identity,3); bool ok=owner!=NULL;
    if (ok) { if (reset) ok=XL6_Q_NATIVE(Reset)(owner->queue); else XL6_Q_NATIVE(Close)(owner->queue); }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) end(owner,3);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    return true;
}
bool XL6_Q_PUBLIC(CloseV1)(const xvalue* value,uint64_t identity) { return transition(value,identity,false); }
bool XL6_Q_PUBLIC(ResetV1)(const xvalue* value,uint64_t identity) { return transition(value,identity,true); }
#else
bool XL6_Q_PUBLIC(CloseV1)(const xvalue* value,uint64_t identity)
{
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    QueueOwner* owner=begin(value,identity,0);bool ok=false;
    if(owner && xl6MutexLockV1(owner->waitMutex,identity)) {
        XL6_Q_NATIVE(Close)(owner->queue);ok=xl6CondBroadcastV1(owner->available,identity);
        if(!xl6MutexUnlockV1(owner->waitMutex,identity))ok=false;
    }
    if(!ok || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(owner)end(owner,0);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6ErrorScopePropagateFailure(outcome);return false;}
    return true;
}
#endif
#undef XL6_Q_NATIVE
#undef XL6_Q_PUBLIC
#undef XL6_Q_JOIN
#undef XL6_Q_JOIN_
