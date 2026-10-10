#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

typedef struct DictOwner {
    volatile int32_t refs;
    xatomic32 busy;
    XL6_DictLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy;
    xtypeddict* storage;
    xtypeddict* retired;
    const void* claim;
    uint64_t cursorIdentity;
    xtypeddictiter iterator;
    const void* current;
    xstrview currentKey;
    bool reverse,started,ended,failed,cleared;
} DictOwner;

static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static bool scopeFinish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    return result.ok;
}
static void discard(xvalue* value)
{
    if (!value) return;
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,false)) {
        xrtErrorFree(incoming); xrtValueRelease(value); (void)scopeFinish(&cleanup,true);
    } else {
        xerror* prior=incoming ? incoming : xrtTakeError();
        xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(prior);
    }
}
static bool emptyPolicy(const XL6_SequenceAdmissionV3* p)
{ return p && !p->ops && !p->data && !p->elementIdentity && !p->transfer && !p->size && !p->type && !p->boxedIdentity; }
static bool sameLayout(const XL6_DictLayoutV1* a,const XL6_DictLayoutV1* b)
{
    return a && b && a->identity==b->identity && a->value.identity==b->value.identity &&
        a->value.elementIdentity==b->value.elementIdentity && a->value.transfer==b->value.transfer &&
        a->value.size==b->value.size && a->value.type==b->value.type;
}
static bool samePolicy(const XL6_SequenceAdmissionV3* a,const XL6_SequenceAdmissionV3* b)
{
    return a && b && a->ops && a->ops==b->ops && a->elementIdentity==b->elementIdentity &&
        a->transfer==b->transfer && a->size==b->size && a->type==b->type &&
        a->boxedIdentity==b->boxedIdentity && (a->data==b->data || a->ops->Equivalent(a->data,b->data));
}
static bool layoutValid(const XL6_DictLayoutV1* layout)
{
    if (!layout || !layout->identity || layout->identity==layout->value.identity) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return xl6SequenceTupleV1(layout->value.identity,layout->value.elementIdentity,
        layout->value.transfer,layout->value.size,layout->value.type);
}
static void cursorEnd(DictOwner* owner)
{
    if (owner->started) xrtTypedDictIterEnd(&owner->iterator);
    owner->started=false; owner->ended=true; owner->current=NULL; owner->currentKey=(xstrview){0};
}
static void destroy(xtypeddict* storage,XL6_SequenceAdmissionV3* policy,void* allocation)
{
    xl6_error_scope scope={0}; xerror* incoming=xrtErrorRef(xrtGetError());
    bool entered=xl6ErrorScopeBeginCleanup(&scope,false); xerror* prior=NULL;
    if (entered) xrtErrorFree(incoming);
    else {
        prior=incoming ? incoming : xrtTakeError();
        xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)});
        xrtClearError();
    }
    /* An unavailable OS scope cannot leak or abort already-owned storage.
     * Capture the earlier outcome before any finalizer can replace it. */
    xrtTypedDictDestroy(storage);
    if (entered && xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(policy); xrtFree(allocation);
    if (entered) (void)scopeFinish(&scope,true);
    else { xrtClearError(); xrtSetErrorTake(prior); }
}
static void drop(ptr opaque,ptr context)
{
    (void)context; DictOwner* owner=opaque; if (!owner) return;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int32_t refs=xrtRefRelease(&owner->refs);
    if (refs<0) abort();
    if (refs) { mutationEnd(&mutation); return; }
    if (owner->retired || xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    cursorEnd(owner); xtypeddict* storage=owner->storage; owner->storage=NULL;
    mutationEnd(&mutation); destroy(storage,&owner->policy,owner);
}
static bool retain(ptr opaque,ptr* output,ptr context)
{
    (void)context; DictOwner* owner=opaque;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    bool success=owner && output && !owner->cleared && xrtRefRetain(&owner->refs)>0;
    if (success) *output=owner; else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return success;
}
static uint64 unsupportedHash(ptr owner,ptr context)
{ (void)owner; (void)context; xrtSetErrorKind(XERR_UNSUPPORTED); return 0; }
static bool unsupportedEqual(ptr left,ptr right,ptr context)
{ (void)left; (void)right; (void)context; xrtSetErrorKind(XERR_UNSUPPORTED); return false; }
static const xvaluehandleops handleOps={retain,drop,unsupportedHash,unsupportedEqual};
static bool pin(DictOwner* owner,uint64_t cursor)
{
    uint32 available=0;
    if (owner && !owner->cleared && owner->storage && owner->cursorIdentity==cursor &&
        xrtAtomic32CompareExchange(&owner->busy,&available,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
        if (xrtRefRetain(&owner->refs)>0) return true;
        xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE);
    }
    xrtSetErrorKind(XERR_STATE); return false;
}
static DictOwner* begin(const xvalue* value,const XL6_DictLayoutV1* layout,uint64_t cursor)
{
    DictOwner* owner=NULL; xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL;
    uint64_t identity=cursor ? cursor : layout ? layout->identity : 0;
    if (value && layout && identity && xrtValueTypeId(value)==identity &&
        xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        DictOwner* candidate=raw;
        if (sameLayout(&candidate->layout,layout) && pin(candidate,cursor)) owner=candidate;
    }
    if (!owner) xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return owner;
}
static void end(DictOwner* owner)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE);
    mutationEnd(&mutation); drop(owner,NULL);
}
static bool finish(xl6_error_scope* scope,DictOwner* owner,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    if (owner) end(owner);
    return scopeFinish(scope,true);
}
static bool slotAdmits(const DictOwner* owner,const void* item)
{
    if (item && owner->policy.ops->Slot(owner->policy.data,item)) return true;
    if (!xrtGetError()) xrtSetErrorKind(item ? XERR_TYPE : XERR_ARGUMENT);
    return false;
}
static bool outputEmpty(const DictOwner* owner,const void* output)
{
    bool empty=output!=NULL;
    if (empty && owner->layout.value.transfer==2)
        empty=!((const xstrview*)output)->Data && !((const xstrview*)output)->Size;
    else if (empty && owner->layout.value.transfer==3) empty=!*(xvalue* const*)output;
    else if (empty && owner->layout.value.transfer==4) empty=!*(xfuture* const*)output;
    if (!empty) xrtSetErrorKind(XERR_ARGUMENT);
    return empty;
}

static bool ownershipCount(const void* opaque,size_t* output)
{
    const DictOwner* owner=opaque;
    if (!owner || !output || owner->refs<=0 || owner->cleared || !owner->storage ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) return false;
    *output=(size_t)owner->refs; return true;
}
typedef struct DictTrace {
    const DictOwner* owner;
    xrtownershipvisitor visit;
    ptr context;
    bool failed;
} DictTrace;
static bool traceEntry(xbytesview key,ptr slot,ptr opaque)
{
    (void)key; DictTrace* trace=opaque; const DictOwner* owner=trace->owner; bool success=true;
    if (owner->layout.value.transfer==3)
        success=trace->visit(xrtValueOwnership(*(xvalue* const*)slot),trace->context);
    else if (owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1)
        success=owner->policy.ops->TraceSlot(owner->policy.data,slot,trace->visit,trace->context);
    if (!success) trace->failed=true;
    return success;
}
static bool ownershipTrace(const void* opaque,xrtownershipvisitor visit,ptr context)
{
    const DictOwner* owner=opaque; size_t refs;
    if (!visit || !ownershipCount(owner,&refs)) return false;
    if (owner->layout.value.transfer<3) return true;
    DictTrace trace={owner,visit,context,false}; size_t count=xrtTypedDictCount(owner->storage);
    size_t visited=xrtMapVisit((xmap*)&owner->storage->Storage,traceEntry,&trace);
    return !trace.failed && visited==count;
}
static const xrtownershipops ownershipOps={ownershipCount,ownershipTrace};
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,ptr context)
{
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL;
    return visit && xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user &&
        visit((xrtownershipref){raw,&ownershipOps},context);
}
static bool ownershipHold(const void* opaque)
{
    DictOwner* owner=(DictOwner*)opaque;
    return owner->refs>0 && owner->refs<INT32_MAX && xrtRefRetain(&owner->refs)>0;
}
static void ownershipDrop(const void* opaque) { drop((ptr)opaque,NULL); }
static bool ownershipClaim(const void* opaque,const void* token)
{
    DictOwner* owner=(DictOwner*)opaque;
    if (!token || owner->cleared || (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void ownershipRestore(const void* opaque,const void* token)
{
    DictOwner* owner=(DictOwner*)opaque;
    if (!token || owner->claim!=token || owner->cleared) abort();
    owner->claim=NULL;
}
static void ownershipClear(const void* opaque,const void* token)
{
    DictOwner* owner=(DictOwner*)opaque;
    if (!token || owner->claim!=token || owner->cleared || owner->retired ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    cursorEnd(owner); owner->retired=owner->storage; owner->storage=NULL; owner->cleared=true;
}
static bool ownershipFinish(const void* opaque,const void* token)
{
    DictOwner* owner=(DictOwner*)opaque;
    if (!token || owner->claim!=token || !owner->cleared) abort();
    xl6_error_scope cleanup={0},work={0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) return false;
    if (!xl6ErrorScopeBegin(&work)) { (void)scopeFinish(&cleanup,false); return false; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xtypeddict* storage=owner->retired; owner->retired=NULL;
    mutationEnd(&mutation); xrtTypedDictDestroy(storage);
    bool success=scopeFinish(&work,true); (void)scopeFinish(&cleanup,true); return success;
}
const xrtownershipadapterv1* xl6DictOwnershipAdapterV2(xrtownershipref ref,
    const XL6_SequenceAdmissionOpsV2* const* allowed,size_t count)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),ownershipHold,ownershipDrop,
        ownershipClaim,ownershipRestore,NULL,ownershipClear,ownershipFinish};
    if (ref.Data && ref.Ops==&ownershipOps) {
        size_t refs; if (!ownershipCount(ref.Data,&refs)) return NULL;
        const DictOwner* owner=ref.Data;
        if (owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) {
            bool found=false;
            for (size_t i=0;allowed && i<count;++i) found=found || allowed[i]==owner->policy.ops;
            if (!found) return NULL;
        }
        return &adapter;
    }
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}
const xrtownershipadapterv1* xl6DictOwnershipAdapterV1(xrtownershipref ref)
{ return xl6DictOwnershipAdapterV2(ref,NULL,0); }

static xvalue* wrap(xtypeddict* storage,const XL6_DictLayoutV1* layout,
    XL6_SequenceAdmissionV3* policy,uint64_t cursor,bool reverse)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) { destroy(storage,policy,NULL); return NULL; }
    DictOwner* owner=xrtMalloc(sizeof(*owner));
    if (!owner) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&scope); destroy(storage,policy,NULL);
        (void)scopeFinish(&scope,true); return NULL;
    }
    *owner=(DictOwner){.refs=1,.layout=*layout,.policy=*policy,.storage=storage,
        .cursorIdentity=cursor,.reverse=reverse};
    *policy=(XL6_SequenceAdmissionV3){0}; xrtAtomic32Init(&owner->busy,0);
    xvalue* result=xrtValueHandleTake((ptr*)&owner,&handleOps,NULL);
    if (!result) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&scope); drop(owner,NULL); (void)scopeFinish(&scope,true); return NULL;
    }
    if (!xrtValueHandleOwnershipBindPhased(result,handleTrace) ||
        !xrtValueTypeIdBind(result,cursor ? cursor : layout->identity)) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); (void)scopeFinish(&scope,true); return NULL;
    }
    if (!scopeFinish(&scope,true)) { discard(result); return NULL; }
    return result;
}
bool xl6DictDomainCopyV1(const XL6_DictLayoutV1* layout,const xvalue* witness,XL6_SequenceAdmissionV3* output)
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
    XL6_Sequence* operation=xl6SequenceBeginRoleV1(witness,role,actual.identity,actual.elementIdentity,
        actual.transfer,actual.size,actual.type);
    if (!operation) goto done;
    success=xrtTypedArrayCount(xl6SequenceStorageV1(operation))==0;
    xl6SequenceEndV1(operation);
    if (!success) xrtSetErrorKind(XERR_ARGUMENT);
done:
    if (success) *output=policy; else xl6SequenceAdmissionDropV3(&policy);
    return success;
}
bool xl6DictAdmitsSchemaV1(const xvalue* value,const XL6_DictLayoutV1* layout,const XL6_SequenceAdmissionV3* policy)
{
    if (!value || !layout || !policy || !policy->ops || xrtValueType(value)!=XVALUE_HANDLE ||
        xrtValueTypeId(value)!=layout->identity) return false;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL; bool success=false;
    if (xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        const DictOwner* owner=raw;
        success=!owner->cleared && !owner->cursorIdentity && sameLayout(&owner->layout,layout) &&
            samePolicy(&owner->policy,policy);
    }
    mutationEnd(&mutation); return success;
}
bool xl6DictSchemaCopyV1(const xvalue* value,XL6_DictLayoutV1* layout,XL6_SequenceAdmissionV3* policy)
{
    if (!value || !layout || !emptyPolicy(policy)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL; bool success=false;
    if (xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        const DictOwner* owner=raw;
        if (owner->cleared || !owner->storage || owner->cursorIdentity ||
            xrtValueTypeId(value)!=owner->layout.identity) xrtSetErrorKind(XERR_STATE);
        else if (owner->policy.ops->Retain(owner->policy.data)) {
            *layout=owner->layout; *policy=owner->policy; success=true;
        }
    } else xrtSetErrorKind(XERR_ARGUMENT);
    mutationEnd(&mutation); return success;
}
xvalue* xl6DictCreateTakeV1(const XL6_DictLayoutV1* layout,xvalue* seed)
{
    xl6_error_scope scope={0};
    if (!(seed ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        discard(seed); return NULL;
    }
    XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (xl6DictDomainCopyV1(layout,seed,&policy)) {
        xtypeddict* storage=xrtTypedDictCreate(xl6SequenceStorageTypeV1(layout->value.transfer,layout->value.type));
        if (storage) result=wrap(storage,layout,&policy,0,false);
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(seed);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
bool xl6DictSetV1(const xvalue* value,const XL6_DictLayoutV1* layout,xstrview key,const void* item)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=begin(value,layout,0);
    return finish(&scope,owner,owner && slotAdmits(owner,item) && xrtTypedDictSet(owner->storage,key,item));
}
bool xl6DictGetV1(const xvalue* value,const XL6_DictLayoutV1* layout,xstrview key,ptr output,bool take,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=begin(value,layout,0); bool success=false,result=false;
    if (!output || !found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && outputEmpty(owner,output)) {
        const void* slot=xrtTypedDictConstGet(owner->storage,key);
        if (!slot) success=!xrtGetError();
        else {
            success=take ? xrtTypedDictTake(owner->storage,key,output) :
                xrtTypeCopyValue(xrtTypedDictItemType(owner->storage),output,slot);
            result=success;
        }
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
bool xl6DictKeyV1(const xvalue* value,const XL6_DictLayoutV1* layout,xstrview key,bool remove,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=begin(value,layout,0); bool success=false,result=false;
    if (!found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        result=xrtTypedDictHas(owner->storage,key);
        success=!xrtGetError() && (!result || !remove || xrtTypedDictRemove(owner->storage,key));
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
bool xl6DictClearV1(const xvalue* value,const XL6_DictLayoutV1* layout)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=begin(value,layout,0);
    return finish(&scope,owner,owner && xrtTypedDictClear(owner->storage));
}
static int64_t measure(const xvalue* value,const XL6_DictLayoutV1* layout,bool capacity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return -1;
    DictOwner* owner=begin(value,layout,0); size_t count=0;
    if (owner) count=capacity ? xrtTypedDictCapacity(owner->storage) : xrtTypedDictCount(owner->storage);
    if (count>INT64_MAX) xrtSetErrorKind(XERR_RANGE);
    return finish(&scope,owner,owner && count<=INT64_MAX) ? (int64_t)count : -1;
}
int64_t xl6DictCountV1(const xvalue* value,const XL6_DictLayoutV1* layout) { return measure(value,layout,false); }
int64_t xl6DictCapacityV1(const xvalue* value,const XL6_DictLayoutV1* layout) { return measure(value,layout,true); }
bool xl6DictReserveV1(const xvalue* value,const XL6_DictLayoutV1* layout,int64_t capacity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=begin(value,layout,0); bool success=false;
    if (capacity<0 || (uint64_t)capacity>SIZE_MAX) xrtSetErrorKind(XERR_RANGE);
    else if (owner) success=xrtTypedDictReserve(owner->storage,(size_t)capacity);
    return finish(&scope,owner,success);
}
bool xl6DictTrimV1(const xvalue* value,const XL6_DictLayoutV1* layout)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=begin(value,layout,0);
    return finish(&scope,owner,owner && xrtTypedDictTrim(owner->storage));
}
static xvalue* clonePinned(DictOwner* owner,uint64_t cursor,bool reverse)
{
    xvalue* result=NULL; XL6_SequenceAdmissionV3 policy={0};
    if (owner->policy.ops->Retain(owner->policy.data)) {
        policy=owner->policy; xtypeddict* storage=xrtTypedDictClone(owner->storage);
        if (storage) result=wrap(storage,&owner->layout,&policy,cursor,reverse);
    }
    xl6SequenceAdmissionDropV3(&policy); return result;
}
static xvalue* clone(const xvalue* value,const XL6_DictLayoutV1* layout,uint64_t cursor,bool reverse)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    DictOwner* owner=begin(value,layout,0); xvalue* result=owner ? clonePinned(owner,cursor,reverse) : NULL;
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6DictCloneV1(const xvalue* value,const XL6_DictLayoutV1* layout)
{ return clone(value,layout,0,false); }
bool xl6DictMergeV1(const xvalue* target,const xvalue* source,const XL6_DictLayoutV1* layout,bool replace)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* a=begin(target,layout,0); DictOwner* b=NULL; bool success=false;
    if (a) {
        ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL;
        if (source && xrtValueTypeId(source)==layout->identity &&
            xrtValueGetHandle(source,&raw,&actual,&user) && actual==&handleOps && raw==a && !user) b=a;
        else b=begin(source,layout,0);
        if (b) {
            if (!samePolicy(&a->policy,&b->policy)) xrtSetErrorKind(XERR_TYPE);
            else success=xrtTypedDictMerge(a->storage,b->storage,replace);
        }
    }
    if (!success || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (b && b!=a) end(b);
    return finish(&scope,a,true);
}
xvalue* xl6DictCursorCreateV1(const xvalue* value,const XL6_DictLayoutV1* layout,uint64_t identity,bool reverse)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    xvalue* result=NULL;
    if (!layout || !identity || identity==layout->identity || identity==layout->value.identity)
        xrtSetErrorKind(XERR_ARGUMENT);
    else result=clone(value,layout,identity,reverse);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
bool xl6DictCursorAdvanceV1(const xvalue* value,const XL6_DictLayoutV1* layout,uint64_t identity,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=identity ? begin(value,layout,identity) : NULL; bool success=false,result=false;
    if (!identity || !found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        if (owner->failed) xrtSetErrorKind(XERR_STATE);
        else if (owner->ended) success=true;
        else {
            if (!owner->started) owner->started=owner->reverse ?
                xrtTypedDictIterRBegin(owner->storage,&owner->iterator) :
                xrtTypedDictIterBegin(owner->storage,&owner->iterator);
            if (owner->started) {
                owner->current=xrtTypedDictIterNext(&owner->iterator,&owner->currentKey);
                success=!xrtGetError() && !scope.failed; result=success && owner->current;
            }
            if (!success) owner->failed=true;
            if (!result) cursorEnd(owner);
        }
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
bool xl6DictCursorReadV1(const xvalue* value,const XL6_DictLayoutV1* layout,uint64_t identity,xstrview* key,ptr output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=identity ? begin(value,layout,identity) : NULL; bool success=false; xstrview copy={0};
    if (!identity || !key || key->Data || key->Size || !output) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && outputEmpty(owner,output)) {
        uintptr_t k=(uintptr_t)key,v=(uintptr_t)output;
        bool overlap=k<=v ? v-k<sizeof(*key) : k-v<owner->layout.value.size;
        if (overlap) xrtSetErrorKind(XERR_ARGUMENT);
        else if (owner->failed || !owner->current) xrtSetErrorKind(XERR_STATE);
        else {
            copy.Size=owner->currentKey.Size; copy.Data=xrtStrDupView(owner->currentKey);
            success=copy.Data && xrtTypeCopyValue(xrtTypedDictItemType(owner->storage),output,owner->current);
        }
    }
    success=finish(&scope,owner,success);
    if (success) *key=copy; else xrtFree((ptr)copy.Data);
    return success;
}
bool xl6DictCursorEndV1(const xvalue* value,const XL6_DictLayoutV1* layout,uint64_t identity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=identity ? begin(value,layout,identity) : NULL;
    if (!identity) xrtSetErrorKind(XERR_ARGUMENT);
    if (owner) cursorEnd(owner);
    return finish(&scope,owner,owner!=NULL);
}
