#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_list_graph_access.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

typedef struct ListOwner {
    volatile int32_t refs;
    xatomic32 busy;
    XL6_ListLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy;
    const xvalueconverter* projection;
    bool projected;
    xtypedlist* storage;
    xtypedlist* retired;
    const void* claim;
    uint64_t cursorIdentity;
    xtypedlistiter iterator;
    const void* current;
    int64_t startKey,currentKey;
    bool reverse,bounded,started,ended,failed,cleared;
} ListOwner;

static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static bool scopeFinish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    return result.ok;
}
/* Retire an unpublished Value after a known failed operation, preserving its
 * first outcome even if cleanup cannot install an OS error-scope context. */
static void discard(xvalue* value)
{
    if (!value) return;
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,true)) {
        xrtErrorFree(incoming); xrtValueRelease(value); (void)scopeFinish(&cleanup,true);
    } else {
        xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(incoming)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(incoming);
    }
}
static bool sameLayout(const XL6_ListLayoutV1* a,const XL6_ListLayoutV1* b)
{
    return a && b && a->identity==b->identity && a->value.identity==b->value.identity &&
        a->value.elementIdentity==b->value.elementIdentity && a->value.transfer==b->value.transfer &&
        a->value.size==b->value.size && a->value.type==b->value.type;
}
static bool samePolicy(const XL6_SequenceAdmissionV3* a,const XL6_SequenceAdmissionV3* b)
{
    return a && b && a->ops && a->ops==b->ops && a->elementIdentity==b->elementIdentity &&
        a->transfer==b->transfer && a->size==b->size && a->type==b->type &&
        a->boxedIdentity==b->boxedIdentity &&
        (a->data==b->data || a->ops->Equivalent(a->data,b->data));
}
static bool emptyPolicy(const XL6_SequenceAdmissionV3* policy)
{
    return policy && !policy->ops && !policy->data && !policy->elementIdentity &&
        !policy->transfer && !policy->size && !policy->type && !policy->boxedIdentity;
}
static bool layoutValid(const XL6_ListLayoutV1* layout)
{
    if (!layout || !layout->identity || layout->identity==layout->value.identity) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return xl6SequenceTupleV1(layout->value.identity,layout->value.elementIdentity,
        layout->value.transfer,layout->value.size,layout->value.type);
}
static void cursorEnd(ListOwner* owner)
{
    if (owner->started) xrtTypedListIterEnd(&owner->iterator);
    owner->started=false; owner->ended=true; owner->current=NULL;
}
static void destroy(xtypedlist* storage,XL6_SequenceAdmissionV3* policy,void* allocation)
{
    xl6_error_scope scope={0};
    xerror* incoming=xrtErrorRef(xrtGetError());
    bool entered=xl6ErrorScopeBeginCleanup(&scope,false);
    /* An OS context failure must not abort or leak an already-owned resource.
     * With no boundary available, preserve its first diagnostic while still
     * completing all allocation-free metadata/storage retirement. */
    xerror* prior=NULL;
    if (entered) xrtErrorFree(incoming);
    else {
        /* Begin may have replaced the diagnostic before installing a scope.
         * Keep the earlier owned error, and latch failure BEFORE callbacks
         * can publish a later error into an existing parent boundary. */
        prior=incoming ? incoming : xrtTakeError();
        xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)});
        xrtClearError();
    }
    xrtTypedListDestroy(storage);
    if (entered && xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(policy);
    xrtFree(allocation);
    if (entered) (void)scopeFinish(&scope,true);
    else { xrtClearError(); xrtSetErrorTake(prior); }
}
static void drop(void* opaque,void* context)
{
    (void)context; ListOwner* owner=opaque; if (!owner) return;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int32_t refs=xrtRefRelease(&owner->refs);
    if (refs<0) abort();
    if (refs) { mutationEnd(&mutation); return; }
    if (owner->retired || xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    cursorEnd(owner);
    xtypedlist* storage=owner->storage; owner->storage=NULL;
    mutationEnd(&mutation);
    destroy(storage,&owner->policy,owner);
}
static bool retain(void* opaque,void** result,void* context)
{
    (void)context; ListOwner* owner=opaque;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    bool success=owner && result && !owner->cleared && xrtRefRetain(&owner->refs)>0;
    if (success) *result=owner; else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return success;
}
static uint64 identityHash(ptr,ptr);
static bool identityEqual(ptr,ptr,ptr);
static const xvaluehandleops handleOps={retain,drop,identityHash,identityEqual};
/* Called inside graph mutation exclusion. Metadata is immutable; an operation
 * owns a separate wrapper pin and releases exclusion before any slot callback. */
static bool pin(ListOwner* owner,uint64_t cursor)
{
    uint32 available=0;
    if (owner && !owner->cleared && owner->storage && owner->cursorIdentity==cursor &&
        xrtAtomic32CompareExchange(&owner->busy,&available,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
        if (xrtRefRetain(&owner->refs)>0) return true;
        xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE);
    }
    xrtSetErrorKind(XERR_STATE); return false;
}
static ListOwner* begin(const xvalue* value,const XL6_ListLayoutV1* expected,uint64_t cursor)
{
    ListOwner* owner=NULL;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
    uint64_t identity=cursor ? cursor : expected ? expected->identity : 0;
    if (value && expected && identity && xrtValueTypeId(value)==identity &&
        xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        ListOwner* candidate=raw;
        if (sameLayout(&candidate->layout,expected) && pin(candidate,cursor)) owner=candidate;
    }
    if (!owner) xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return owner;
}
static void end(ListOwner* owner)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE);
    mutationEnd(&mutation); drop(owner,NULL);
}
static bool finish(xl6_error_scope* scope,ListOwner* owner,bool success)
{
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    if (owner) end(owner);
    return scopeFinish(scope,true);
}
static bool slotAdmits(const ListOwner* owner,const void* item)
{
    if (item && owner->policy.ops->Slot(owner->policy.data,item)) return true;
    if (!xrtGetError()) xrtSetErrorKind(item ? XERR_TYPE : XERR_ARGUMENT);
    return false;
}
static bool sameDomain(const ListOwner* a,const ListOwner* b)
{ return sameLayout(&a->layout,&b->layout) && samePolicy(&a->policy,&b->policy); }
static bool compatible(const ListOwner* a,const ListOwner* b)
{
    if (a && b && sameDomain(a,b)) return true;
    xrtSetErrorKind(XERR_TYPE); return false;
}
static ListOwner* beginOther(const xvalue* value,const XL6_ListLayoutV1* layout,ListOwner* first)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
    if (first && value && xrtValueTypeId(value)==layout->identity &&
        xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && !user && raw==first) return first;
    return begin(value,layout,0);
}
static bool finishPair(xl6_error_scope* scope,ListOwner* a,ListOwner* b,bool success)
{
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    if (b && b!=a) end(b);
    return finish(scope,a,true);
}
static bool equalSlot(const ListOwner* owner,const void* left,const void* right,bool* output)
{
    if (owner->layout.value.transfer==3) {
        /* Raw Value equality's false means inequality OR failure. Observe one
         * complete callback boundary, including its independent NULL failure
         * bit, before accepting a result or comparing a subsequent slot. */
        xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
        bool equal=xl6SequenceReferenceEqualV1(*(xvalue* const*)left,*(xvalue* const*)right);
        if (!scopeFinish(&scope,true)) return false;
        *output=equal;
    }
    else if (owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1)
        *output=*(xfuture* const*)left==*(xfuture* const*)right;
    else {
        int order=0;
        if (!xrtTypeCompareValue(owner->layout.value.type,left,right,&order)) return false;
        *output=order==0;
    }
    return !xrtGetError();
}
static bool hashSlot(const ListOwner* owner,const void* slot,uint64* output)
{
    if (owner->layout.value.transfer==3) {
        xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
        const xvalue* value=*(xvalue* const*)slot;
        uint64 hash=0; bool success=xrtValueHash(value ? value : xrtValueNull(),&hash);
        if (!scopeFinish(&scope,success)) return false;
        *output=hash; return true;
    }
    if (owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) {
        *output=(uint64)(uintptr_t)*(xfuture* const*)slot; return true;
    }
    return xrtTypeHashValue(owner->layout.value.type,slot,output);
}
/* Pairing sorted iterators is O(n), including sparse/negative/INT64_MIN keys;
 * no ordinal search or boxed temporary is used for either typed slot. */
static bool equalPinned(const ListOwner* a,const ListOwner* b,bool* output)
{
    *output=false;
    if (!sameDomain(a,b)) return true;
    if (a==b) { *output=true; return true; }
    if (xrtTypedListCount(a->storage)!=xrtTypedListCount(b->storage)) return true;
    XL6_CollectionIdentityFrameV1 frame={0};
    if (!xl6CollectionIdentityEnterV1(&frame)) return false;
    xtypedlistiter left={0},right={0}; bool leftStarted=false,rightStarted=false,success=false;
    if (!xrtTypedListIterBegin(a->storage,&left)) goto done;
    leftStarted=true;
    if (!xrtTypedListIterBegin(b->storage,&right)) goto done;
    rightStarted=true; success=true; *output=true;
    for (;;) {
        int64 keyA=0,keyB=0;
        const void* valueA=xrtTypedListIterNext(&left,&keyA);
        const void* valueB=xrtTypedListIterNext(&right,&keyB);
        if (xrtGetError()) { success=false; break; }
        if (!valueA || !valueB) { *output=valueA==valueB; break; }
        if (keyA!=keyB) { *output=false; break; }
        if (!equalSlot(a,valueA,valueB,output)) { success=false; break; }
        if (!*output) break;
    }
done:
    if (rightStarted) xrtTypedListIterEnd(&right);
    if (leftStarted) xrtTypedListIterEnd(&left);
    xl6CollectionIdentityLeaveV1(&frame); return success;
}
static uint64 mix(uint64 hash,uint64 item)
{ return hash ^ (item+UINT64_C(0x9E3779B97F4A7C15)+(hash<<6)+(hash>>2)); }
static bool hashPinned(const ListOwner* owner,uint64_t* output)
{
    XL6_CollectionIdentityFrameV1 frame={0};
    if (!xl6CollectionIdentityEnterV1(&frame)) return false;
    xtypedlistiter iterator={0}; bool started=false,success=false;
    uint64 hash=mix(owner->layout.identity,owner->layout.value.elementIdentity);
    hash=mix(hash,(uint64)xrtTypedListCount(owner->storage));
    if (!xrtTypedListIterBegin(owner->storage,&iterator)) goto done;
    started=true; success=true;
    for (;;) {
        int64 key=0; const void* slot=xrtTypedListIterNext(&iterator,&key);
        if (xrtGetError()) { success=false; break; }
        if (!slot) break;
        uint64 item=0;
        if (!hashSlot(owner,slot,&item)) { success=false; break; }
        if (xrtGetError()) { success=false; break; }
        hash=mix(mix(hash,(uint64)key),item);
    }
done:
    if (started) xrtTypedListIterEnd(&iterator);
    xl6CollectionIdentityLeaveV1(&frame);
    if (success) *output=hash;
    return success;
}
/* XRT invokes these with captured private Handle fields while its Value shell
 * is BUSY. Querying GetHandle there would fail. Pin the genuine owner instead.
 * XRT's payload-only Hash/Equal callback ABI needs a diagnostic on failure;
 * retain the checked first failure in the language boundary, then publish an
 * allocation-free secondary diagnostic only if the primary has no object. */
static ListOwner* beginCaptured(ListOwner* owner)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    if (owner && owner->cursorIdentity) { xrtSetErrorKind(XERR_UNSUPPORTED); owner=NULL; }
    else if (!pin(owner,0)) owner=NULL;
    mutationEnd(&mutation); return owner;
}
static void callbackFailure(void)
{ if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); }
static uint64 identityHash(ptr opaque,ptr user)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) { callbackFailure(); return 0; }
    ListOwner* owner=user ? NULL : beginCaptured(opaque); uint64_t hash=0;
    if (user) xrtSetErrorKind(XERR_ARGUMENT);
    bool success=owner && hashPinned(owner,&hash);
    if (!finish(&scope,owner,success)) { callbackFailure(); return 0; }
    return hash;
}
static bool identityEqual(ptr left,ptr right,ptr user)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) { callbackFailure(); return false; }
    ListOwner* a=user ? NULL : beginCaptured(left);
    ListOwner* b=a ? (left==right ? a : beginCaptured(right)) : NULL;
    bool equal=false,success=a && b && equalPinned(a,b,&equal);
    if (user) xrtSetErrorKind(XERR_ARGUMENT);
    if (!finishPair(&scope,a,b,success)) { callbackFailure(); return false; }
    return equal;
}
bool xl6ListEqualsV1(const xvalue* left,const xvalue* right,const XL6_ListLayoutV1* layout,bool* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* a=begin(left,layout,0); ListOwner* b=a ? beginOther(right,layout,a) : NULL;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    bool equal=false,success=output && a && b && compatible(a,b) && equalPinned(a,b,&equal);
    success=finishPair(&scope,a,b,success);
    if (success) *output=equal;
    return success;
}
bool xl6ListHashV1(const xvalue* value,const XL6_ListLayoutV1* layout,uint64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=begin(value,layout,0); uint64_t hash=0;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    bool success=output && owner && hashPinned(owner,&hash);
    success=finish(&scope,owner,success);
    if (success) *output=hash;
    return success;
}
bool xl6ListContainsV1(const xvalue* value,const XL6_ListLayoutV1* layout,const void* item,bool* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=begin(value,layout,0); bool success=false,found=false,started=false,entered=false;
    xtypedlistiter iterator={0}; XL6_CollectionIdentityFrameV1 frame={0};
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    if (!output || !owner || !slotAdmits(owner,item)) goto done;
    if (!xl6CollectionIdentityEnterV1(&frame)) goto done;
    entered=true;
    if (!xrtTypedListIterBegin(owner->storage,&iterator)) goto done;
    started=true; success=true;
    for (;;) {
        const void* slot=xrtTypedListIterNext(&iterator,NULL);
        if (xrtGetError()) { success=false; break; }
        if (!slot) break;
        if (!equalSlot(owner,slot,item,&found)) { success=false; break; }
        if (found) break;
    }
done:
    if (started) xrtTypedListIterEnd(&iterator);
    if (entered) xl6CollectionIdentityLeaveV1(&frame);
    success=finish(&scope,owner,success);
    if (success) *output=found;
    return success;
}
bool xl6ListMergeV1(const xvalue* target,const xvalue* source,const XL6_ListLayoutV1* layout,bool replace)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* a=begin(target,layout,0); ListOwner* b=a ? beginOther(source,layout,a) : NULL;
    bool success=a && b && compatible(a,b) && xrtTypedListMerge(a->storage,b->storage,replace);
    return finishPair(&scope,a,b,success);
}

static bool ownershipCount(const void* opaque,size_t* result)
{
    const ListOwner* owner=opaque;
    if (!owner || !result || owner->refs<=0 || owner->cleared || !owner->storage ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) return false;
    *result=(size_t)owner->refs; return true;
}
typedef struct ListTrace {
    const ListOwner* owner;
    xrtownershipvisitor visit;
    void* context;
    bool failed;
} ListTrace;
static bool traceEntry(int64 key,ptr slot,ptr opaque)
{
    (void)key; ListTrace* trace=opaque; const ListOwner* owner=trace->owner;
    bool success=true;
    if (owner->layout.value.transfer==3)
        success=trace->visit(xrtValueOwnership(*(xvalue* const*)slot),trace->context);
    else if (owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1)
        success=owner->policy.ops->TraceSlot(owner->policy.data,slot,trace->visit,trace->context);
    if (!success) trace->failed=true;
    return success;
}
static bool ownershipTrace(const void* opaque,xrtownershipvisitor visit,void* context)
{
    const ListOwner* owner=opaque; size_t refs;
    if (!visit || !ownershipCount(owner,&refs)) return false;
    if (owner->layout.value.transfer<3) return true;
    ListTrace trace={owner,visit,context,false};
    size_t count=xrtTypedListCount(owner->storage);
    size_t visited=xrtIntMapVisit((xintmap*)&owner->storage->Storage,traceEntry,&trace);
    return !trace.failed && visited==count;
}
static const xrtownershipops ownershipOps={ownershipCount,ownershipTrace};
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
    return visit && xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user &&
        visit((xrtownershipref){raw,&ownershipOps},context);
}
static bool ownershipHold(const void* opaque)
{
    ListOwner* owner=(ListOwner*)opaque;
    return owner->refs>0 && owner->refs<INT32_MAX && xrtRefRetain(&owner->refs)>0;
}
static void ownershipDrop(const void* opaque) { drop((void*)opaque,NULL); }
static bool ownershipClaim(const void* opaque,const void* token)
{
    ListOwner* owner=(ListOwner*)opaque;
    if (!token || owner->cleared || (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void ownershipRestore(const void* opaque,const void* token)
{
    ListOwner* owner=(ListOwner*)opaque;
    if (!token || owner->claim!=token || owner->cleared) abort();
    owner->claim=NULL;
}
static void ownershipClear(const void* opaque,const void* token)
{
    ListOwner* owner=(ListOwner*)opaque;
    if (!token || owner->claim!=token || owner->cleared || owner->retired ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    cursorEnd(owner); owner->retired=owner->storage; owner->storage=NULL; owner->cleared=true;
}
static bool ownershipFinish(const void* opaque,const void* token)
{
    ListOwner* owner=(ListOwner*)opaque;
    if (!token || owner->claim!=token || !owner->cleared) abort();
    xl6_error_scope cleanup={0},work={0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) return false;
    if (!xl6ErrorScopeBegin(&work)) { (void)scopeFinish(&cleanup,false); return false; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xtypedlist* storage=owner->retired; owner->retired=NULL;
    mutationEnd(&mutation);
    xrtTypedListDestroy(storage);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&work);
    xl6_error_result result=xl6ErrorScopeEnd(&work);
    bool success=result.ok;
    if (!success) xl6ErrorScopePropagateFailure(result);
    (void)scopeFinish(&cleanup,true); return success;
}
const xrtownershipadapterv1* xl6ListOwnershipAdapterV2(xrtownershipref ref,
    const XL6_SequenceAdmissionOpsV2* const* allowed,size_t count)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),ownershipHold,ownershipDrop,
        ownershipClaim,ownershipRestore,NULL,ownershipClear,ownershipFinish};
    if (ref.Data && ref.Ops==&ownershipOps) {
        size_t refs; if (!ownershipCount(ref.Data,&refs)) return NULL;
        const ListOwner* owner=ref.Data;
        if (owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) {
            bool found=false;
            for (size_t i=0;allowed && i<count;++i) found=found || allowed[i]==owner->policy.ops;
            if (!found) return NULL;
        }
        return &adapter;
    }
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}
const xrtownershipadapterv1* xl6ListOwnershipAdapterV1(xrtownershipref ref)
{ return xl6ListOwnershipAdapterV2(ref,NULL,0); }

static xvalue* wrap(xtypedlist* storage,const XL6_ListLayoutV1* layout,
    XL6_SequenceAdmissionV3* policy,uint64_t cursor,bool reverse,bool bounded,int64_t start,
    const xvalueconverter* projection,bool projected)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) { destroy(storage,policy,NULL); return NULL; }
    ListOwner* owner=xrtMalloc(sizeof(*owner));
    if (!owner) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&scope); destroy(storage,policy,NULL);
        (void)scopeFinish(&scope,true); return NULL;
    }
    *owner=(ListOwner){.refs=1,.layout=*layout,.policy=*policy,.storage=storage,
        .cursorIdentity=cursor,.reverse=reverse,.bounded=bounded,.startKey=start,
        .projection=projection,.projected=projected};
    *policy=(XL6_SequenceAdmissionV3){0}; xrtAtomic32Init(&owner->busy,0);
    xvalue* result=xrtValueHandleTake((void**)&owner,&handleOps,NULL);
    if (!result) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&scope); drop(owner,NULL);
        (void)scopeFinish(&scope,true); return NULL;
    }
    if (!xrtValueHandleOwnershipBindPhased(result,handleTrace) ||
        !xrtValueTypeIdBind(result,cursor ? cursor : layout->identity)) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result);
        (void)scopeFinish(&scope,true); return NULL;
    }
    if (!scopeFinish(&scope,true)) { xrtValueRelease(result); return NULL; }
    return result;
}
bool xl6ListDomainCopyV1(const XL6_ListLayoutV1* layout,const xvalue* witness,XL6_SequenceAdmissionV3* output)
{
    if (!witness || !emptyPolicy(output)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 actual={0};
    XL6_SequenceAdmissionV3 policy={0}; bool success=false;
    if (!layoutValid(layout) || !xl6SequenceSchemaCopyV1(witness,&role,&actual,&policy)) goto done;
    const XL6_SequenceLayoutV1* value=&layout->value;
    if (role!=XL6_SEQUENCE_ARRAY_V1 || actual.identity!=value->identity ||
        actual.elementIdentity!=value->elementIdentity || actual.transfer!=value->transfer ||
        actual.size!=value->size || actual.type!=value->type ||
        (value->transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1 && policy.ops!=xl6ArrayFutureAdmissionOpsV2Get())) {
        xrtSetErrorKind(XERR_TYPE); goto done;
    }
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(witness,role,actual.identity,actual.elementIdentity,
        actual.transfer,actual.size,actual.type);
    if (!pin) goto done;
    success=xrtTypedArrayCount(xl6SequenceStorageV1(pin))==0;
    xl6SequenceEndV1(pin);
    if (!success) xrtSetErrorKind(XERR_ARGUMENT);
done:
    if (success) *output=policy; else xl6SequenceAdmissionDropV3(&policy);
    return success;
}
bool xl6ListAdmitsSchemaV1(const xvalue* value,const XL6_ListLayoutV1* layout,const XL6_SequenceAdmissionV3* policy)
{
    if (!value || !layout || !policy || !policy->ops || xrtValueType(value)!=XVALUE_HANDLE ||
        xrtValueTypeId(value)!=layout->identity) return false;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
    bool success=false;
    if (xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        const ListOwner* owner=raw;
        success=!owner->cleared && !owner->cursorIdentity && sameLayout(&owner->layout,layout) &&
            samePolicy(&owner->policy,policy);
    }
    mutationEnd(&mutation); return success;
}
bool xl6ListSchemaCopyV1(const xvalue* value,XL6_ListLayoutV1* layout,XL6_SequenceAdmissionV3* policy)
{
    if (!value || !layout || !emptyPolicy(policy)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
    bool success=false;
    if (xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        const ListOwner* owner=raw;
        if (owner->cleared || !owner->storage || owner->cursorIdentity ||
            xrtValueTypeId(value)!=owner->layout.identity) xrtSetErrorKind(XERR_STATE);
        else if (owner->policy.ops->Retain(owner->policy.data)) {
            *layout=owner->layout; *policy=owner->policy; success=true;
        }
    } else xrtSetErrorKind(XERR_ARGUMENT);
    mutationEnd(&mutation); return success;
}
xvalue* xl6ListCreateTakeV1(const XL6_ListLayoutV1* layout,xvalue* seed)
{
    xl6_error_scope scope={0};
    if (!(seed ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        discard(seed); return NULL;
    }
    XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (xl6ListDomainCopyV1(layout,seed,&policy)) {
        xtypedlist* storage=xrtTypedListCreate(xl6SequenceStorageTypeV1(layout->value.transfer,layout->value.type));
        if (storage) result=wrap(storage,layout,&policy,0,false,false,0,NULL,false);
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(seed);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
bool xl6ListSetV1(const xvalue* value,const XL6_ListLayoutV1* layout,int64_t key,const void* item)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=begin(value,layout,0);
    return finish(&scope,owner,owner && slotAdmits(owner,item) && xrtTypedListSet(owner->storage,key,item));
}
bool xl6ListAppendV1(const xvalue* value,const XL6_ListLayoutV1* layout,const void* item,int64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=begin(value,layout,0); int64 key=0;
    bool success=owner && output && slotAdmits(owner,item) && xrtTypedListAppend(owner->storage,item,&key);
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    success=finish(&scope,owner,success);
    if (success) *output=key;
    return success;
}
bool xl6ListGetV1(const xvalue* value,const XL6_ListLayoutV1* layout,int64_t key,void* output,bool take,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=begin(value,layout,0); bool success=false,result=false;
    if (!output || !found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        const void* slot=xrtTypedListConstGet(owner->storage,key);
        if (!slot) success=!xrtGetError();
        else {
            success=take ? xrtTypedListTake(owner->storage,key,output) :
                xrtTypeCopyValue(xrtTypedListItemType(owner->storage),output,slot);
            result=success;
        }
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
static bool keyPinned(ListOwner* owner,int64_t key,bool remove,bool* found)
{
    bool present=xrtTypedListHas(owner->storage,key);
    if (xrtGetError() || (present && remove && !xrtTypedListRemove(owner->storage,key))) return false;
    *found=present; return true;
}
bool xl6ListKeyV1(const xvalue* value,const XL6_ListLayoutV1* layout,int64_t key,bool remove,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=begin(value,layout,0); bool success=false,result=false;
    if (!found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) success=keyPinned(owner,key,remove,&result);
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
bool xl6ListClearV1(const xvalue* value,const XL6_ListLayoutV1* layout)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=begin(value,layout,0);
    return finish(&scope,owner,owner && xrtTypedListClear(owner->storage));
}
int64_t xl6ListCountV1(const xvalue* value,const XL6_ListLayoutV1* layout)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return -1;
    ListOwner* owner=begin(value,layout,0); size_t count=owner ? xrtTypedListCount(owner->storage) : 0;
    if (count>INT64_MAX) xrtSetErrorKind(XERR_RANGE);
    return finish(&scope,owner,owner && count<=INT64_MAX) ? (int64_t)count : -1;
}
int64_t xl6ListTrimV1(const xvalue* value,const XL6_ListLayoutV1* layout,int64_t retainEmpty)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return -1;
    ListOwner* owner=begin(value,layout,0);
    bool success=owner && retainEmpty>=0 && (uint64_t)retainEmpty<=SIZE_MAX;
    if (!success && owner) xrtSetErrorKind(XERR_RANGE);
    size_t released=success ? xrtTypedListTrim(owner->storage,(size_t)retainEmpty) : 0;
    if (released>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); success=false; }
    return finish(&scope,owner,success) ? (int64_t)released : -1;
}
static xvalue* clonePinned(ListOwner* owner,uint64_t cursor,bool reverse,bool bounded,int64_t start,
    xl6_error_scope* scope)
{
    xvalue* result=NULL; XL6_SequenceAdmissionV3 policy={0};
    if (owner && owner->policy.ops->Retain(owner->policy.data)) {
        policy=owner->policy; xtypedlist* storage=xrtTypedListClone(owner->storage);
        if (storage) result=wrap(storage,&owner->layout,&policy,cursor,reverse,bounded,start,
            owner->projection,owner->projected);
    }
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6SequenceAdmissionDropV3(&policy);
    return result;
}
static xvalue* clone(const xvalue* value,const XL6_ListLayoutV1* layout,uint64_t cursor,
    bool reverse,bool bounded,int64_t start)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    ListOwner* owner=begin(value,layout,0);
    xvalue* result=owner ? clonePinned(owner,cursor,reverse,bounded,start,&scope) : NULL;
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    if (!finish(&scope,owner,true)) { xrtValueRelease(result); return NULL; }
    return result;
}
xvalue* xl6ListCloneV1(const xvalue* value,const XL6_ListLayoutV1* layout)
{ return clone(value,layout,0,false,false,0); }
xvalue* xl6ListCursorCreateV1(const xvalue* value,const XL6_ListLayoutV1* layout,uint64_t identity,
    bool reverse,bool bounded,int64_t start)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    xvalue* result=NULL;
    if (!layout || !identity || identity==layout->identity || identity==layout->value.identity) {
        xrtSetErrorKind(XERR_ARGUMENT);
    } else result=clone(value,layout,identity,reverse,bounded,start);
    if (!scopeFinish(&scope,result!=NULL)) { xrtValueRelease(result); return NULL; }
    return result;
}
static bool cursorAdvancePinned(ListOwner* owner,bool* found)
{
    *found=false;
    if (owner->failed) { xrtSetErrorKind(XERR_STATE); return false; }
    if (owner->ended) return true;
    if (!owner->started) {
        owner->started=owner->bounded ?
            (owner->reverse ? xrtTypedListIterRFrom(owner->storage,owner->startKey,&owner->iterator) :
                xrtTypedListIterFrom(owner->storage,owner->startKey,&owner->iterator)) :
            (owner->reverse ? xrtTypedListIterRBegin(owner->storage,&owner->iterator) :
                xrtTypedListIterBegin(owner->storage,&owner->iterator));
        if (!owner->started) { owner->failed=true; cursorEnd(owner); return false; }
    }
    owner->current=xrtTypedListIterNext(&owner->iterator,&owner->currentKey);
    bool success=!xrtGetError(); *found=success && owner->current;
    if (!success) owner->failed=true;
    if (!owner->current || !success) cursorEnd(owner);
    return success;
}
bool xl6ListCursorAdvanceV1(const xvalue* value,const XL6_ListLayoutV1* layout,uint64_t identity,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=identity ? begin(value,layout,identity) : NULL;
    bool success=false,result=false;
    if (!identity || !found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) success=cursorAdvancePinned(owner,&result);
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
bool xl6ListCursorReadV1(const xvalue* value,const XL6_ListLayoutV1* layout,uint64_t identity,
    int64_t* key,void* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=identity ? begin(value,layout,identity) : NULL; bool success=false; int64 actual=0;
    if (!identity || !key || !output) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        if (owner->failed || !owner->current) xrtSetErrorKind(XERR_STATE);
        else {
            actual=owner->currentKey;
            success=xrtTypeCopyValue(xrtTypedListItemType(owner->storage),output,owner->current);
        }
    }
    success=finish(&scope,owner,success);
    if (success) *key=actual;
    return success;
}
bool xl6ListCursorEndV1(const xvalue* value,const XL6_ListLayoutV1* layout,uint64_t identity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=identity ? begin(value,layout,identity) : NULL;
    if (!identity) xrtSetErrorKind(XERR_ARGUMENT);
    if (owner) cursorEnd(owner);
    return finish(&scope,owner,owner!=NULL);
}

/* Projection binding is immutable resident metadata. Dynamic use aliases the
 * same shell/storage; an explicit data boundary below makes a sparse wire
 * snapshot instead. No generated callback/context is authorized by this ABI. */
xvalue* xl6ListEraseV1(const xvalue* value,const XL6_ListLayoutV1* layout,const xvalueconverter* codec)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    ListOwner* owner=begin(value,layout,0); xvalue* result=NULL;
    if (owner) {
        if ((codec && (codec->Context || !codec->ToTyped || !codec->FromTyped)) ||
            (owner->layout.value.transfer==3 && codec)) xrtSetErrorKind(XERR_ARGUMENT);
        else {
            const xvalueconverter* selected=owner->layout.value.transfer==3 ? xl6ArrayReferenceConverterV1() : codec;
            if (owner->projected && owner->projection!=selected) xrtSetErrorKind(XERR_STATE);
            else {
                owner->projection=selected; owner->projected=true;
                result=xrtValueRetain(value);
            }
        }
    }
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6ListProjectionTakeV1(xvalue* value,const XL6_ListLayoutV1* layout,const xvalueconverter* codec)
{
    xl6_error_scope scope={0};
    if (!(value ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        discard(value); return NULL;
    }
    xvalue* result=xl6ListEraseV1(value,layout,codec);
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(value);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
/* 0 unrelated; -1 genuine but refused; 1 operation pin acquired. A same-owner
 * source alias is detected without trying to acquire that owner's BUSY twice. */
static int erasedBegin(const xvalue* value,ListOwner** output,ListOwner* already)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int status=0; ListOwner* owner=NULL; bool unbound=false;
    if (value && xrtValueType(value)==XVALUE_HANDLE) {
        void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
        if (!xrtValueGetHandle(value,&raw,&actual,&user)) status=-1;
        else if (actual==&handleOps) {
            status=-1;
            if (!raw || user) xrtSetErrorKind(XERR_STATE);
            else {
                ListOwner* candidate=raw;
                if (candidate->cursorIdentity) xrtSetErrorKind(XERR_TYPE);
                else if (xrtValueTypeId(value)!=candidate->layout.identity) xrtSetErrorKind(XERR_STATE);
                else if (candidate==already) { owner=candidate; status=1; }
                else if (pin(candidate,0)) {
                    owner=candidate;
                    if (!owner->projected) unbound=true; else status=1;
                }
            }
        }
    }
    mutationEnd(&mutation);
    if (unbound) { end(owner); owner=NULL; xrtSetErrorKind(XERR_STATE); }
    if (status==1) *output=owner;
    return status;
}
static ListOwner* erased(const xvalue* value)
{
    ListOwner* owner=NULL;
    if (!erasedBegin(value,&owner,NULL)) xrtSetErrorKind(XERR_TYPE);
    return owner;
}
xvalue* xl6ListErasedCloneV1(const xvalue* value)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    ListOwner* owner=erased(value);
    xvalue* result=owner ? clonePinned(owner,0,false,false,0,&scope) : NULL;
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return NULL; }
    return result;
}
int xl6ListErasedProbeV1(const xvalue* value)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int status=0;
    if (value && xrtValueType(value)==XVALUE_HANDLE) {
        void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
        status=!xrtValueGetHandle(value,&raw,&actual,&user) ? -1 : actual==&handleOps ? 1 : 0;
    }
    mutationEnd(&mutation);
    return status;
}
/* These transient adapters protect XRT's payload-only codec boundary. A
 * callback may publish only a failed bit, even while returning true/a Value.
 * Return refusal to XRT BEFORE it commits a decoded slot or exposes a result.
 * The stored converter has NULL Context; only this synchronous adapter uses
 * a native stack context, never retained by the list or XRT typed operations. */
static bool decode(const xvalue* value,const xrttype* type,ptr output,ptr context)
{
    const ListOwner* owner=context;
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) { callbackFailure(); return false; }
    bool success=owner->projection->ToTyped(value,type,output,NULL);
    if (success && !slotAdmits(owner,output)) success=false;
    if (!scopeFinish(&scope,success)) { callbackFailure(); return false; }
    return true;
}
static xvalue* encode(const xrttype* type,const void* slot,ptr context)
{
    const ListOwner* owner=context;
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) { callbackFailure(); return NULL; }
    xvalue* result=owner->projection->FromTyped(type,slot,NULL);
    if (!result || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    if (!scopeFinish(&scope,result!=NULL)) { callbackFailure(); return NULL; }
    return result;
}
static const xvalueconverter* adapter(ListOwner* owner,xvalueconverter* codec)
{
    if (!owner->projection) return NULL;
    *codec=(xvalueconverter){.ToTyped=decode,.FromTyped=encode,.Context=owner}; return codec;
}
static bool admitsValue(const ListOwner* owner,const xvalue* value)
{
    if (value && owner->policy.ops->Value(owner->policy.data,value)) return true;
    if (!xrtGetError()) xrtSetErrorKind(value ? XERR_TYPE : XERR_ARGUMENT);
    return false;
}
static xvalue* projectedSlot(ListOwner* owner,const void* slot)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    xvalueconverter codec={0};
    xvalue* result=xrtValueFromTyped(xrtTypedListItemType(owner->storage),slot,adapter(owner,&codec));
    if (result) {
        uint64_t identity=owner->policy.boxedIdentity;
        xvaluetype tag=xrtValueType(result);
        if ((identity && tag!=XVALUE_NULL && tag!=XVALUE_BOOL && !xrtValueTypeIdBind(result,identity)) ||
            !admitsValue(owner,result)) {
            xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
        }
    }
    if (!result || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    (void)scopeFinish(&scope,result!=NULL); return result;
}
int xl6ListGraphBeginV1(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,XL6_ListGraphViewV1* view)
{
    if (!view || view->owner) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    if (ops!=&handleOps) return 0;
    if (!raw || user) { xrtSetErrorKind(XERR_STATE); return -1; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ListOwner* owner=raw; bool success=false,rejected=false;
    if (owner->cursorIdentity || identity!=owner->layout.identity) xrtSetErrorKind(XERR_STATE);
    else if (pin(owner,0)) {
        if (!owner->projected) rejected=true;
        else { *view=(XL6_ListGraphViewV1){owner,owner->layout,owner->storage}; success=true; }
    }
    mutationEnd(&mutation);
    if (rejected) { end(owner); xrtSetErrorKind(XERR_STATE); }
    return success ? 1 : -1;
}
void xl6ListGraphEndV1(XL6_ListGraphViewV1* view)
{
    if (!view || !view->owner) abort();
    ListOwner* owner=view->owner; *view=(XL6_ListGraphViewV1){0}; end(owner);
}
static ListOwner* graphOwner(const XL6_ListGraphViewV1* view)
{
    ListOwner* owner=view ? view->owner : NULL;
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE) || owner->cleared ||
        !owner->projected || owner->storage!=view->storage || !sameLayout(&owner->layout,&view->layout)) {
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    return owner;
}
xvalue* xl6ListGraphEmptyV1(const XL6_ListGraphViewV1* view)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    ListOwner* owner=graphOwner(view); XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (owner && owner->policy.ops->Retain(owner->policy.data)) {
        policy=owner->policy;
        xtypedlist* storage=xrtTypedListCreate(xl6SequenceStorageTypeV1(owner->layout.value.transfer,owner->layout.value.type));
        if (storage) result=wrap(storage,&owner->layout,&policy,0,false,false,0,owner->projection,true);
    }
    xl6SequenceAdmissionDropV3(&policy);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6ListGraphProjectSlotV1(const XL6_ListGraphViewV1* view,const void* slot)
{
    ListOwner* owner=graphOwner(view);
    if (!owner) return NULL;
    if (owner->layout.value.transfer==3 || !slot) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    return projectedSlot(owner,slot);
}
bool xl6ListErasedReadV1(const xvalue* value,int64_t key,bool take,xvalue** output,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=erased(value); xvalue* result=NULL; bool success=false,present=false;
    if (!output || *output || !found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        present=xrtTypedListHas(owner->storage,key);
        success=!xrtGetError();
        if (success && present) {
            result=projectedSlot(owner,xrtTypedListConstGet(owner->storage,key));
            success=result && (!take || xrtTypedListRemove(owner->storage,key));
        }
    }
    if (!success || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    success=finish(&scope,owner,success);
    if (success) { *output=result; *found=present; }
    else discard(result);
    return success;
}
static bool writeValue(ListOwner* owner,int64_t key,const xvalue* item,bool append,int64_t* output)
{
    if (!admitsValue(owner,item)) return false;
    xvalueconverter codec={0}; const xvalueconverter* selected=adapter(owner,&codec);
    return append ? xrtTypedListAppendValue(owner->storage,item,output,selected) :
        xrtTypedListSetValue(owner->storage,key,item,selected);
}
bool xl6ListErasedSetV1(const xvalue* value,int64_t key,const xvalue* item)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=erased(value);
    return finish(&scope,owner,owner && writeValue(owner,key,item,false,NULL));
}
bool xl6ListErasedAppendV1(const xvalue* value,const xvalue* item,int64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=erased(value); int64_t key=0;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    bool success=output && owner && writeValue(owner,0,item,true,&key);
    success=finish(&scope,owner,success);
    if (success) *output=key;
    return success;
}
bool xl6ListErasedCountV1(const xvalue* value,int64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=erased(value); size_t count=owner ? xrtTypedListCount(owner->storage) : 0;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    else if (count>INT64_MAX) xrtSetErrorKind(XERR_RANGE);
    bool success=finish(&scope,owner,owner && output && count<=INT64_MAX);
    if (success) *output=(int64_t)count;
    return success;
}
bool xl6ListErasedKeyV1(const xvalue* value,int64_t key,bool remove,bool* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=erased(value); bool found=false;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    bool success=owner && output && keyPinned(owner,key,remove,&found);
    success=finish(&scope,owner,success);
    if (success) *output=found;
    return success;
}
bool xl6ListErasedClearV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=erased(value);
    return finish(&scope,owner,owner && xrtTypedListClear(owner->storage));
}
bool xl6ListErasedTrimV1(const xvalue* value,int64_t retainEmpty,int64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=erased(value); size_t released=0;
    bool success=owner && output && retainEmpty>=0 && (uint64_t)retainEmpty<=SIZE_MAX;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && !success) xrtSetErrorKind(XERR_RANGE);
    if (success) released=xrtTypedListTrim(owner->storage,(size_t)retainEmpty);
    if (released>INT64_MAX) { xrtSetErrorKind(XERR_RANGE); success=false; }
    success=finish(&scope,owner,success);
    if (success) *output=(int64_t)released;
    return success;
}
bool xl6ListErasedContainsV1(const xvalue* value,const xvalue* item,bool* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=erased(value); bool success=false,found=false,started=false;
    xtypedlistiter iterator={0};
    if (!item || !output) xrtSetErrorKind(XERR_ARGUMENT);
    if (!owner || !item || !output || !xrtTypedListIterBegin(owner->storage,&iterator)) goto done;
    started=true; success=true;
    while (!found) {
        const void* slot=xrtTypedListIterNext(&iterator,NULL);
        if (xrtGetError() || scope.failed) { success=false; break; }
        if (!slot) break;
        xvalue* current=projectedSlot(owner,slot);
        if (!current || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
        if (!scope.failed) found=xrtValueEqual(current,item);
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
        xrtValueRelease(current);
        if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
        if (scope.failed) { success=false; break; }
    }
done:
    if (started) xrtTypedListIterEnd(&iterator);
    success=finish(&scope,owner,success);
    if (success) *output=found;
    return success;
}
int xl6ListIteratorCreateErasedV1(const xvalue* value,uint64_t identity,
    xvalue** output,XL6_ListLayoutV1* outputLayout)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return -1;
    ListOwner* owner=NULL; int status=erasedBegin(value,&owner,NULL);
    if (status!=1) return finish(&scope,NULL,status==0) && status==0 ? 0 : -1;
    xvalue* result=NULL; XL6_ListLayoutV1 layout=owner->layout;
    if (!output || *output || !outputLayout || !identity ||
        identity==layout.identity || identity==layout.value.identity) xrtSetErrorKind(XERR_ARGUMENT);
    else result=clonePinned(owner,identity,false,false,0,&scope);
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return -1; }
    *output=result; *outputLayout=layout; return 1;
}
bool xl6ListIteratorAdvanceValueV1(const xvalue* value,const XL6_ListLayoutV1* layout,
    uint64_t identity,bool* outputFound,xvalue** output,int64_t* outputKey)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=identity ? begin(value,layout,identity) : NULL;
    bool success=false,found=false; xvalue* result=NULL; int64_t key=0;
    if (!identity || !outputFound || !output || *output || !outputKey) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && !owner->projected) xrtSetErrorKind(XERR_STATE);
    else if (owner) {
        success=cursorAdvancePinned(owner,&found);
        if (success && found) {
            key=owner->currentKey; result=projectedSlot(owner,owner->current);
            success=result!=NULL;
        }
        if (!success || xrtGetError() || scope.failed) { owner->failed=true; cursorEnd(owner); }
    }
    if (!finish(&scope,owner,success)) { discard(result); return false; }
    *output=result; *outputFound=found; *outputKey=key; return true;
}
int xl6ListIteratorSnapshotErasedV1(const xvalue* value,xvalue** output,
    XL6_ListLayoutV1* outputLayout,uint64_t* outputIdentity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return -1;
    ListOwner* owner=NULL; int status=erasedBegin(value,&owner,NULL);
    if (status!=1) return finish(&scope,NULL,status==0) && status==0 ? 0 : -1;
    xvalue* result=NULL; XL6_ListLayoutV1 layout=owner->layout;
    uint64_t identity=layout.identity;
    /* At most three exclusions; unsigned wrap is defined. Cursor identity is
     * private metadata, not a language type or a new nominal collection. */
    do { ++identity; } while (!identity || identity==layout.identity || identity==layout.value.identity);
    if (!output || *output || !outputLayout || !outputIdentity) xrtSetErrorKind(XERR_ARGUMENT);
    else result=clonePinned(owner,identity,false,false,0,&scope);
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return -1; }
    *output=result; *outputLayout=layout; *outputIdentity=identity; return 1;
}
/* One-level explicit data projection. Resident reference children retain
 * their exact identities; a closed graph plan, not this storage boundary,
 * owns recursive persistence/restore. Sorted native iteration is O(n). */
static xvalue* wirePinned(ListOwner* owner)
{
    xvalue* result=xrtValueIntMap(); xtypedlistiter iterator={0}; bool started=false,success=false;
    if (!result || !xrtTypedListIterBegin(owner->storage,&iterator)) goto done;
    started=true; success=true;
    for (;;) {
        int64_t key=0; const void* slot=xrtTypedListIterNext(&iterator,&key);
        if (xrtGetError()) { success=false; break; }
        if (!slot) break;
        xvalue* item=projectedSlot(owner,slot);
        if (!item || !xrtValueIntMapSetTake(result,key,&item)) {
            success=false; discard(item);
            break;
        }
    }
done:
    if (started) xrtTypedListIterEnd(&iterator);
    if (!success) {
        discard(result);
        return NULL;
    }
    return result;
}
xvalue* xl6ListToWireV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    ListOwner* owner=erased(value); xvalue* result=owner ? wirePinned(owner) : NULL;
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6ListImportTakeV1(xvalue* seed,const xvalue* source)
{
    xl6_error_scope scope={0};
    if (!(seed ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        discard(seed); return NULL;
    }
    ListOwner* target=NULL; ListOwner* input=NULL; xvalue* wire=NULL;
    xvalueiter iterator={0}; bool started=false,success=false;
    if (!seed || !source || seed==source) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    target=erased(seed);
    if (!target) goto done;
    if (xrtTypedListCount(target->storage)) { xrtSetErrorKind(XERR_STATE); goto done; }
    int native=erasedBegin(source,&input,target);
    if (native<0) goto done;
    if (input==target) { input=NULL; xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    if (native) {
        if (sameDomain(target,input) && target->projection==input->projection) {
            success=xrtTypedListMerge(target->storage,input->storage,false); goto done;
        }
        wire=wirePinned(input);
        if (!wire) goto done;
        end(input); input=NULL; source=wire;
    }
    if (xrtValueType(source)!=XVALUE_INT_MAP) { xrtSetErrorKind(XERR_TYPE); goto done; }
    if (!xrtValueIterBegin(source,&iterator)) goto done;
    started=true;
    for (;;) {
        xvaluekey key={0}; xvalue* item=NULL;
        xvalueiterresult status=xrtValueIterAdvance(&iterator,&key,&item);
        if (status==XVALUE_ITER_END) { success=true; break; }
        if (status!=XVALUE_ITER_ITEM) break;
        if (key.Type!=XVALUE_KEY_INT) { xrtSetErrorKind(XERR_STATE); break; }
        if (!writeValue(target,key.Integer,item,false,NULL)) break;
    }
done:
    if (!success || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtValueIterEnd(&iterator);
    xrtValueRelease(wire);
    if (input) end(input);
    if (target) end(target);
    if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (scope.failed) { xrtValueRelease(seed); seed=NULL; }
    if (!scopeFinish(&scope,success)) return NULL;
    return seed;
}
