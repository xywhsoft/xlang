#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_set_graph_access.h"
#include "src/runtime/xl6_set_key_policy.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_scalar_domain.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

typedef struct SetOwner {
    volatile int32_t refs;
    xatomic32 busy;
    XL6_SetLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy;
    const xvalueconverter* projection;
    bool projected;
    xtypedset* storage;
    xtypedset* retired;
    const void* claim;
    uint64_t cursorIdentity;
    xtypedsetiter iterator;
    const void* current;
    bool reverse,started,ended,failed,cleared;
} SetOwner;
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
static bool sameLayout(const XL6_SetLayoutV1* a,const XL6_SetLayoutV1* b)
{
    return a && b && a->identity==b->identity && a->value.identity==b->value.identity &&
        a->value.elementIdentity==b->value.elementIdentity && a->value.transfer==b->value.transfer &&
        a->value.size==b->value.size && a->value.type==b->value.type;
}
static bool samePolicy(const XL6_SequenceAdmissionV3* a,const XL6_SequenceAdmissionV3* b)
{
    return a && b && a->ops && a->ops==b->ops && a->elementIdentity==b->elementIdentity &&
        a->transfer==b->transfer && a->size==b->size && a->type==b->type &&
        a->boxedIdentity==b->boxedIdentity && a->scalarDomain==b->scalarDomain && (a->data==b->data || a->ops->Equivalent(a->data,b->data));
}
static bool emptyPolicy(const XL6_SequenceAdmissionV3* p)
{ return p && !p->ops && !p->data && !p->elementIdentity && !p->transfer && !p->size && !p->type && !p->boxedIdentity && !p->scalarDomain; }
static bool layoutValid(const XL6_SetLayoutV1* layout)
{
    if (!layout || !layout->identity || layout->identity==layout->value.identity) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    const XL6_SequenceLayoutV1* v=&layout->value;
    return xl6SequenceTupleV1(v->identity,v->elementIdentity,v->transfer,v->size,v->type);
}
static bool domainCopy(const XL6_SetLayoutV1* layout,const xvalue* witness,XL6_SequenceAdmissionV3* output)
{
    if (!witness || !emptyPolicy(output)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 actual={0}; XL6_SequenceAdmissionV3 p={0};
    bool ok=false;
    if (!layoutValid(layout) || !xl6SequenceSchemaCopyV1(witness,&role,&actual,&p)) goto done;
    const XL6_SequenceLayoutV1* v=&layout->value;
    if (role!=XL6_SEQUENCE_ARRAY_V1 || actual.identity!=v->identity || actual.elementIdentity!=v->elementIdentity ||
        actual.transfer!=v->transfer || actual.size!=v->size || actual.type!=v->type ||
        (v->transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1 && p.ops!=xl6ArrayFutureAdmissionOpsV2Get())) {
        xrtSetErrorKind(XERR_TYPE); goto done;
    }
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(witness,role,actual.identity,actual.elementIdentity,
        actual.transfer,actual.size,actual.type);
    if (!pin) goto done;
    ok=xrtTypedArrayCount(xl6SequenceStorageV1(pin))==0; xl6SequenceEndV1(pin);
    if (!ok) xrtSetErrorKind(XERR_ARGUMENT);
done:
    if (ok) *output=p; else xl6SequenceAdmissionDropV3(&p);
    return ok;
}
static void cursorEnd(SetOwner* owner)
{
    if (owner->started) xrtTypedSetIterEnd(&owner->iterator);
    owner->started=false; owner->ended=true; owner->current=NULL;
}
/* Cleanup always retires actual owned storage/metadata, even if the OS cannot
 * install an error boundary. Preserve and propagate the original first error. */
static void destroy(xtypedset* storage,XL6_SequenceAdmissionV3* policy,void* allocation)
{
    xl6_error_scope scope={0}; xerror* incoming=xrtErrorRef(xrtGetError());
    bool entered=xl6ErrorScopeBeginCleanup(&scope,false); xerror* prior=NULL;
    if (entered) xrtErrorFree(incoming);
    else {
        prior=incoming ? incoming : xrtTakeError(); xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)}); xrtClearError();
    }
    xrtTypedSetDestroy(storage);
    if (entered && xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(policy); xrtFree(allocation);
    if (entered) (void)scopeFinish(&scope,true);
    else { xrtClearError(); xrtSetErrorTake(prior); }
}
static void drop(ptr opaque,ptr user)
{
    (void)user; SetOwner* owner=opaque; if (!owner) return;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int32_t count=xrtRefRelease(&owner->refs); if (count<0) abort();
    if (count) { mutationEnd(&mutation); return; }
    if (owner->retired || xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    cursorEnd(owner); xtypedset* storage=owner->storage; owner->storage=NULL;
    mutationEnd(&mutation); destroy(storage,&owner->policy,owner);
}
static bool retain(ptr opaque,ptr* result,ptr user)
{
    (void)user; SetOwner* owner=opaque; xrtownershipscope mutation={0}; mutationBegin(&mutation);
    bool ok=owner && result && !owner->cleared && xrtRefRetain(&owner->refs)>0;
    if (ok) *result=owner; else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return ok;
}
static uint64 identityHash(ptr,ptr);
static bool identityEqual(ptr,ptr,ptr);
static const xvaluehandleops handleOps={retain,drop,identityHash,identityEqual};
static bool pin(SetOwner* owner,uint64_t cursor)
{
    uint32 available=0;
    if (owner && !owner->cleared && owner->storage && owner->cursorIdentity==cursor &&
        xrtAtomic32CompareExchange(&owner->busy,&available,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
        if (xrtRefRetain(&owner->refs)>0) return true;
        xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE);
    }
    xrtSetErrorKind(XERR_STATE); return false;
}
static SetOwner* begin(const xvalue* value,const XL6_SetLayoutV1* layout,uint64_t cursor)
{
    SetOwner* owner=NULL; ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    uint64_t identity=cursor ? cursor : layout ? layout->identity : 0;
    if (value && layout && identity && xrtValueTypeId(value)==identity &&
        xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        SetOwner* candidate=raw;
        if (sameLayout(&candidate->layout,layout) && pin(candidate,cursor)) owner=candidate;
    }
    if (!owner) xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return owner;
}
static void end(SetOwner* owner)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE); mutationEnd(&mutation); drop(owner,NULL);
}
static bool finish(xl6_error_scope* scope,SetOwner* owner,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    if (owner) end(owner);
    return scopeFinish(scope,true);
}
static bool slotAdmits(const SetOwner* owner,const void* item)
{
    if (item && owner->policy.ops->Slot(owner->policy.data,item)) return true;
    if (!xrtGetError()) xrtSetErrorKind(item ? XERR_TYPE : XERR_ARGUMENT);
    return false;
}
static SetOwner* beginOther(const xvalue* value,const XL6_SetLayoutV1* layout,SetOwner* first)
{
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
    if (first && value && xrtValueTypeId(value)==layout->identity && xrtValueGetHandle(value,&raw,&ops,&user) &&
        ops==&handleOps && !user && raw==first) return first;
    return begin(value,layout,0);
}
static bool finishPair(xl6_error_scope* scope,SetOwner* a,SetOwner* b,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    if (b && b!=a) end(b);
    return finish(scope,a,true);
}
static bool compatible(const SetOwner* a,const SetOwner* b)
{
    if (a && b && sameLayout(&a->layout,&b->layout) && samePolicy(&a->policy,&b->policy)) return true;
    xrtSetErrorKind(XERR_TYPE); return false;
}
static bool equalPinned(SetOwner* a,SetOwner* b,bool* output)
{
    *output=false;
    if (!sameLayout(&a->layout,&b->layout) || !samePolicy(&a->policy,&b->policy)) return true;
    XL6_CollectionIdentityFrameV1 frame={0}; if (!xl6CollectionIdentityEnterV1(&frame)) return false;
    *output=xrtTypedSetEquals(a->storage,b->storage);
    xl6CollectionIdentityLeaveV1(&frame); return !xrtGetError();
}
static bool hashPinned(SetOwner* owner,uint64_t* output)
{
    XL6_CollectionIdentityFrameV1 frame={0}; if (!xl6CollectionIdentityEnterV1(&frame)) return false;
    xtypedsetiter iterator={0}; bool started=false,ok=false; uint64 aggregate=0;
    if (!xrtTypedSetIterBegin(owner->storage,&iterator)) goto done;
    started=true; ok=true;
    for (;;) {
        const void* slot=xrtTypedSetIterNext(&iterator); if (xrtGetError()) { ok=false; break; }
        if (!slot) break;
        uint64 hash=0;
        if (owner->storage->KeyPolicy) hash=owner->storage->KeyPolicy->Hash(slot,owner->storage->KeyPolicy->UserData);
        else if (!xrtTypeHashValue(owner->storage->ItemType,slot,&hash)) { ok=false; break; }
        if (xrtGetError()) { ok=false; break; }
        aggregate+=hash; /* Commutative: equality must not depend on insertion order. */
    }
done:
    if (started) xrtTypedSetIterEnd(&iterator);
    xl6CollectionIdentityLeaveV1(&frame);
    if (ok) {
        uint64 domain[4]={owner->layout.identity,owner->layout.value.elementIdentity,
            (uint64)xrtTypedSetCount(owner->storage),aggregate};
        *output=xrtHash64(domain,sizeof(domain));
    }
    return ok && !xrtGetError();
}
static SetOwner* beginCaptured(ptr opaque)
{
    SetOwner* owner=opaque; xrtownershipscope mutation={0}; mutationBegin(&mutation);
    if (owner && owner->cursorIdentity) { xrtSetErrorKind(XERR_UNSUPPORTED); owner=NULL; }
    else if (!pin(owner,0)) owner=NULL;
    mutationEnd(&mutation); return owner;
}
static void callbackFailure(void) { if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); }
static void callbackBeginFailure(void)
{ xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); callbackFailure(); }
static uint64 identityHash(ptr opaque,ptr user)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) { callbackBeginFailure(); return 0; }
    SetOwner* owner=user ? NULL : beginCaptured(opaque); uint64 hash=0;
    if (user) xrtSetErrorKind(XERR_ARGUMENT);
    if (!finish(&scope,owner,owner && hashPinned(owner,&hash))) { callbackFailure(); return 0; }
    return hash;
}
static bool identityEqual(ptr left,ptr right,ptr user)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) { callbackBeginFailure(); return false; }
    SetOwner* a=user ? NULL : beginCaptured(left),*b=a ? (left==right ? a : beginCaptured(right)) : NULL;
    bool equal=false; if (user) xrtSetErrorKind(XERR_ARGUMENT);
    if (!finishPair(&scope,a,b,a && b && equalPinned(a,b,&equal))) { callbackFailure(); return false; }
    return equal;
}

static bool ownershipCount(const void* opaque,size_t* output)
{
    const SetOwner* owner=opaque;
    if (!owner || !output || owner->refs<=0 || owner->cleared || !owner->storage ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) return false;
    *output=(size_t)owner->refs; return true;
}
typedef struct SetTrace {
    const SetOwner* owner; xrtownershipvisitor visit; void* context; bool failed;
} SetTrace;
static bool traceEntry(const void* slot,ptr context)
{
    SetTrace* trace=context; const SetOwner* owner=trace->owner; bool ok=true;
    if (owner->layout.value.transfer==3) ok=trace->visit(xrtValueOwnership(*(xvalue* const*)slot),trace->context);
    else if (owner->layout.value.transfer>=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1)
        ok=owner->policy.ops->TraceSlot(owner->policy.data,slot,trace->visit,trace->context);
    if (!ok) trace->failed=true;
    return ok;
}
static bool ownershipTrace(const void* opaque,xrtownershipvisitor visit,void* context)
{
    const SetOwner* owner=opaque; size_t refs;
    if (!visit || !ownershipCount(owner,&refs)) return false;
    if (owner->layout.value.transfer<3) return true;
    SetTrace trace={owner,visit,context,false}; size_t count=xrtTypedSetCount(owner->storage);
    size_t seen=xrtSetVisit((xset*)&owner->storage->Storage,traceEntry,&trace);
    return !trace.failed && seen==count;
}
static const xrtownershipops ownershipOps={ownershipCount,ownershipTrace};
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
    return visit && xrtValueGetHandle(value,&raw,&ops,&user) && ops==&handleOps && raw && !user &&
        visit((xrtownershipref){raw,&ownershipOps},context);
}
static bool ownershipHold(const void* opaque)
{
    SetOwner* owner=(SetOwner*)opaque;
    return owner->refs>0 && owner->refs<INT32_MAX && xrtRefRetain(&owner->refs)>0;
}
static void ownershipDrop(const void* opaque) { drop((void*)opaque,NULL); }
static bool ownershipClaim(const void* opaque,const void* token)
{
    SetOwner* owner=(SetOwner*)opaque;
    if (!token || owner->cleared || (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void ownershipRestore(const void* opaque,const void* token)
{
    SetOwner* owner=(SetOwner*)opaque;
    if (!token || owner->claim!=token || owner->cleared) abort();
    owner->claim=NULL;
}
static void ownershipClear(const void* opaque,const void* token)
{
    SetOwner* owner=(SetOwner*)opaque;
    if (!token || owner->claim!=token || owner->cleared || owner->retired ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    cursorEnd(owner); owner->retired=owner->storage; owner->storage=NULL; owner->cleared=true;
}
static bool ownershipFinish(const void* opaque,const void* token)
{
    SetOwner* owner=(SetOwner*)opaque;
    if (!token || owner->claim!=token || !owner->cleared) abort();
    xl6_error_scope cleanup={0},work={0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) return false;
    if (!xl6ErrorScopeBegin(&work)) { (void)scopeFinish(&cleanup,false); return false; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xtypedset* storage=owner->retired; owner->retired=NULL; mutationEnd(&mutation);
    xrtTypedSetDestroy(storage);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&work);
    xl6_error_result result=xl6ErrorScopeEnd(&work); bool ok=result.ok;
    if (!ok) xl6ErrorScopePropagateFailure(result);
    (void)scopeFinish(&cleanup,true); return ok;
}
const xrtownershipadapterv1* xl6SetOwnershipAdapterV2(xrtownershipref ref,
    const XL6_SequenceAdmissionOpsV2* const* allowed,size_t count)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),ownershipHold,ownershipDrop,
        ownershipClaim,ownershipRestore,NULL,ownershipClear,ownershipFinish};
    if (ref.Data && ref.Ops==&ownershipOps) {
        size_t refs; if (!ownershipCount(ref.Data,&refs)) return NULL;
        const SetOwner* owner=ref.Data;
        if (owner->layout.value.transfer>=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) {
            bool found=false;
            for (size_t i=0;allowed && i<count;++i) found=found || allowed[i]==owner->policy.ops;
            if (!found) return NULL;
        }
        return &adapter;
    }
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}
const xrtownershipadapterv1* xl6SetOwnershipAdapterV1(xrtownershipref ref)
{ return xl6SetOwnershipAdapterV2(ref,NULL,0); }

static xvalue* wrap(xtypedset* storage,const XL6_SetLayoutV1* layout,XL6_SequenceAdmissionV3* policy,
    uint64_t cursor,bool reverse,const xvalueconverter* projection,bool projected)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) { destroy(storage,policy,NULL); return NULL; }
    SetOwner* owner=xrtMalloc(sizeof(*owner));
    if (!owner) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&scope); destroy(storage,policy,NULL);
        (void)scopeFinish(&scope,true); return NULL;
    }
    *owner=(SetOwner){.refs=1,.layout=*layout,.policy=*policy,.storage=storage,.cursorIdentity=cursor,
        .reverse=reverse,.projection=projection,.projected=projected};
    *policy=(XL6_SequenceAdmissionV3){0}; xrtAtomic32Init(&owner->busy,0);
    xvalue* result=xrtValueHandleTake((ptr*)&owner,&handleOps,NULL);
    if (!result) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&scope); drop(owner,NULL); (void)scopeFinish(&scope,true); return NULL;
    }
    if (!xrtValueHandleOwnershipBindPhased(result,handleTrace) || !xrtValueTypeIdBind(result,cursor ? cursor : layout->identity)) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result); result=NULL;
    }
    if (!scopeFinish(&scope,result!=NULL)) { xrtValueRelease(result); return NULL; }
    return result;
}
static void discard(xvalue* value)
{
    if (!value) return;
    xerror* prior=xrtErrorRef(xrtGetError()); xl6_error_scope scope={0};
    if (xl6ErrorScopeBeginCleanup(&scope,true)) {
        xrtErrorFree(prior); xrtValueRelease(value); (void)scopeFinish(&scope,true);
    } else {
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(prior);
    }
}
xvalue* xl6SetCreateTakeV1(const XL6_SetLayoutV1* layout,xvalue* witness)
{
    xl6_error_scope scope={0};
    if (!(witness ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        discard(witness); return NULL;
    }
    XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (domainCopy(layout,witness,&policy)) {
        const xrttype* type=xl6SequenceStorageTypeV1(layout->value.transfer,layout->value.type);
        const xtypedsetkeypolicy* keys=layout->value.transfer==3 ? xl6SetReferenceKeyPolicyV1() :
            layout->value.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1 ? xl6SetFutureKeyPolicyV1() :
            layout->value.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 ? xl6SequenceNativeValuePolicyV1(&policy)->content : NULL;
        xtypedset* storage=keys ? xrtTypedSetCreateWithKeyPolicy(type,keys) : xrtTypedSetCreate(type);
        if (storage) result=wrap(storage,layout,&policy,0,false,NULL,false);
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(witness);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
bool xl6SetAdmitsSchemaV1(const xvalue* value,const XL6_SetLayoutV1* layout,const XL6_SequenceAdmissionV3* policy)
{
    if (!value || !layout || !policy || !policy->ops || xrtValueType(value)!=XVALUE_HANDLE ||
        xrtValueTypeId(value)!=layout->identity) return false;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL; bool ok=false;
    if (xrtValueGetHandle(value,&raw,&ops,&user) && ops==&handleOps && raw && !user) {
        const SetOwner* owner=raw;
        ok=!owner->cleared && !owner->cursorIdentity && sameLayout(&owner->layout,layout) && samePolicy(&owner->policy,policy);
    }
    mutationEnd(&mutation); return ok;
}
bool xl6SetSchemaCopyV1(const xvalue* value,XL6_SetLayoutV1* layout,XL6_SequenceAdmissionV3* policy)
{
    if (!value || !layout || !emptyPolicy(policy)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL; bool ok=false;
    if (xrtValueGetHandle(value,&raw,&ops,&user) && ops==&handleOps && raw && !user) {
        const SetOwner* owner=raw;
        if (owner->cleared || !owner->storage || owner->cursorIdentity || xrtValueTypeId(value)!=owner->layout.identity)
            xrtSetErrorKind(XERR_STATE);
        else if (owner->policy.ops->Retain(owner->policy.data)) { *layout=owner->layout; *policy=owner->policy; ok=true; }
    } else xrtSetErrorKind(XERR_ARGUMENT);
    mutationEnd(&mutation); return ok;
}
bool xl6SetAddV1(const xvalue* value,const XL6_SetLayoutV1* layout,const void* item,bool* outputNew)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=begin(value,layout,0); bool added=false;
    if (!outputNew) xrtSetErrorKind(XERR_ARGUMENT);
    bool ok=outputNew && owner && slotAdmits(owner,item) && xrtTypedSetGetOrAdd(owner->storage,item,&added);
    ok=finish(&scope,owner,ok); if (ok) *outputNew=added; return ok;
}
bool xl6SetKeyV1(const xvalue* value,const XL6_SetLayoutV1* layout,const void* item,bool remove,bool* outputFound)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=begin(value,layout,0); bool ok=false,found=false;
    if (!outputFound) xrtSetErrorKind(XERR_ARGUMENT);
    if (outputFound && owner && slotAdmits(owner,item)) {
        found=xrtTypedSetHas(owner->storage,item);
        ok=!xrtGetError() && (!remove || !found || xrtTypedSetRemove(owner->storage,item));
    }
    ok=finish(&scope,owner,ok); if (ok) *outputFound=found; return ok;
}
bool xl6SetGetV1(const xvalue* value,const XL6_SetLayoutV1* layout,const void* key,ptr output,bool take,bool* outputFound)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=begin(value,layout,0); bool ok=false,found=false;
    if (!output || !outputFound) xrtSetErrorKind(XERR_ARGUMENT);
    if (output && outputFound && owner && slotAdmits(owner,key)) {
        const void* slot=xrtTypedSetGet(owner->storage,key);
        if (!slot) ok=!xrtGetError();
        else {
            ok=take ? xrtTypedSetTake(owner->storage,key,output) : xrtTypeCopyValue(owner->storage->ItemType,output,slot);
            found=ok;
        }
    }
    ok=finish(&scope,owner,ok); if (ok) *outputFound=found; return ok;
}
bool xl6SetClearV1(const xvalue* value,const XL6_SetLayoutV1* layout)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=begin(value,layout,0); return finish(&scope,owner,owner && xrtTypedSetClear(owner->storage));
}
bool xl6SetReserveV1(const xvalue* value,const XL6_SetLayoutV1* layout,int64_t count)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=begin(value,layout,0); bool valid=count>=0 && (uint64_t)count<=SIZE_MAX;
    if (!valid) xrtSetErrorKind(XERR_RANGE);
    return finish(&scope,owner,owner && valid && xrtTypedSetReserve(owner->storage,(size_t)count));
}
bool xl6SetTrimV1(const xvalue* value,const XL6_SetLayoutV1* layout)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=begin(value,layout,0); return finish(&scope,owner,owner && xrtTypedSetTrim(owner->storage));
}
int64_t xl6SetCountV1(const xvalue* value,const XL6_SetLayoutV1* layout,bool capacity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return -1;
    SetOwner* owner=begin(value,layout,0);
    size_t count=owner ? (capacity ? xrtTypedSetCapacity(owner->storage) : xrtTypedSetCount(owner->storage)) : 0;
    if (count>INT64_MAX) xrtSetErrorKind(XERR_RANGE);
    return finish(&scope,owner,owner && count<=INT64_MAX) ? (int64_t)count : -1;
}
static xvalue* clonePinned(SetOwner* owner,uint64_t cursor,bool reverse,xl6_error_scope* scope)
{
    XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (owner && owner->policy.ops->Retain(owner->policy.data)) {
        policy=owner->policy; xtypedset* storage=xrtTypedSetClone(owner->storage);
        if (storage) result=wrap(storage,&owner->layout,&policy,cursor,reverse,owner->projection,owner->projected);
    }
    if (!result) xl6ErrorScopeCaptureFailure(scope);
    xl6SequenceAdmissionDropV3(&policy);
    return result;
}
static xvalue* clone(const xvalue* value,const XL6_SetLayoutV1* layout,uint64_t cursor,bool reverse)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    SetOwner* owner=begin(value,layout,0); xvalue* result=clonePinned(owner,cursor,reverse,&scope);
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6SetCloneV1(const xvalue* value,const XL6_SetLayoutV1* layout) { return clone(value,layout,0,false); }
bool xl6SetEqualsV1(const xvalue* left,const xvalue* right,const XL6_SetLayoutV1* layout,bool* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* a=begin(left,layout,0),*b=a ? beginOther(right,layout,a) : NULL; bool equal=false;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    bool ok=output && a && b && compatible(a,b) && equalPinned(a,b,&equal);
    ok=finishPair(&scope,a,b,ok); if (ok) *output=equal; return ok;
}
bool xl6SetHashV1(const xvalue* value,const XL6_SetLayoutV1* layout,uint64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=begin(value,layout,0); uint64 hash=0;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    bool ok=output && owner && hashPinned(owner,&hash);
    ok=finish(&scope,owner,ok); if (ok) *output=hash; return ok;
}
bool xl6SetMergeV1(const xvalue* target,const xvalue* source,const XL6_SetLayoutV1* layout)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* a=begin(target,layout,0),*b=a ? beginOther(source,layout,a) : NULL;
    return finishPair(&scope,a,b,a && b && compatible(a,b) && xrtTypedSetMerge(a->storage,b->storage));
}
static xvalue* algebraPinned(SetOwner* a,SetOwner* b,XL6_SetAlgebraV1 operation,xl6_error_scope* scope)
{
    XL6_SequenceAdmissionV3 policy={0}; xtypedset* storage=NULL; xvalue* result=NULL;
    if (operation<XL6_SET_UNION_V1 || operation>XL6_SET_SYMMETRIC_DIFFERENCE_V1) xrtSetErrorKind(XERR_ARGUMENT);
    else if (a && b && compatible(a,b) && a->policy.ops->Retain(a->policy.data)) {
        policy=a->policy;
        switch (operation) {
        case XL6_SET_UNION_V1: storage=xrtTypedSetUnion(a->storage,b->storage); break;
        case XL6_SET_INTERSECTION_V1: storage=xrtTypedSetIntersection(a->storage,b->storage); break;
        case XL6_SET_DIFFERENCE_V1: storage=xrtTypedSetDifference(a->storage,b->storage); break;
        case XL6_SET_SYMMETRIC_DIFFERENCE_V1: storage=xrtTypedSetSymmetricDifference(a->storage,b->storage); break;
        }
        if (storage) result=wrap(storage,&a->layout,&policy,0,false,a->projection,a->projected);
    }
    if (!result) xl6ErrorScopeCaptureFailure(scope);
    xl6SequenceAdmissionDropV3(&policy);
    return result;
}
xvalue* xl6SetAlgebraV1(const xvalue* left,const xvalue* right,const XL6_SetLayoutV1* layout,XL6_SetAlgebraV1 operation)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    SetOwner* a=begin(left,layout,0),*b=a ? beginOther(right,layout,a) : NULL;
    xvalue* result=algebraPinned(a,b,operation,&scope);
    if (!finishPair(&scope,a,b,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6SetCursorCreateV1(const xvalue* value,const XL6_SetLayoutV1* layout,uint64_t identity,bool reverse)
{
    if (!layout || !identity || identity==layout->identity || identity==layout->value.identity) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    return clone(value,layout,identity,reverse);
}
static bool cursorAdvancePinned(SetOwner* owner,bool* found)
{
    bool ok=false; *found=false;
    if (!owner->failed) {
        ok=true;
        if (!owner->ended) {
            if (!owner->started) {
                ok=owner->reverse ? xrtTypedSetIterRBegin(owner->storage,&owner->iterator) :
                    xrtTypedSetIterBegin(owner->storage,&owner->iterator);
                owner->started=ok;
            }
            if (ok) {
                owner->current=xrtTypedSetIterNext(&owner->iterator); ok=!xrtGetError();
                *found=ok && owner->current;
            }
            if (!*found) cursorEnd(owner);
        }
    } else xrtSetErrorKind(XERR_STATE);
    return ok;
}
bool xl6SetCursorAdvanceV1(const xvalue* value,const XL6_SetLayoutV1* layout,uint64_t identity,bool* outputFound)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=identity ? begin(value,layout,identity) : NULL; bool found=false;
    if (!identity || !outputFound) xrtSetErrorKind(XERR_ARGUMENT);
    bool ok=owner && outputFound && cursorAdvancePinned(owner,&found);
    if (owner && (!ok || xrtGetError() || scope.failed)) { owner->failed=true; cursorEnd(owner); }
    ok=finish(&scope,owner,ok); if (ok) *outputFound=found; return ok;
}
bool xl6SetCursorReadV1(const xvalue* value,const XL6_SetLayoutV1* layout,uint64_t identity,ptr output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=identity ? begin(value,layout,identity) : NULL; bool ok=false;
    if (!identity || !output) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && !owner->failed && owner->current) ok=xrtTypeCopyValue(owner->storage->ItemType,output,owner->current);
    else if (owner) xrtSetErrorKind(XERR_STATE);
    if (owner && (!ok || xrtGetError() || scope.failed)) { owner->failed=true; cursorEnd(owner); }
    return finish(&scope,owner,ok);
}

xvalue* xl6SetProjectionTakeV1(xvalue* value,const XL6_SetLayoutV1* layout,const xvalueconverter* codec)
{
    xl6_error_scope scope={0};
    if (!(value ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        discard(value); return NULL;
    }
    SetOwner* owner=begin(value,layout,0); xvalue* result=NULL;
    if (owner) {
        if ((codec && (codec->Context || !codec->ToTyped || !codec->FromTyped)) ||
            (owner->layout.value.transfer==3 && codec)) xrtSetErrorKind(XERR_ARGUMENT);
        else {
            const xvalueconverter* selected=owner->layout.value.transfer==3 ? xl6ArrayReferenceConverterV1() : codec;
            if (owner->projected && owner->projection!=selected) xrtSetErrorKind(XERR_STATE);
            else { owner->projection=selected; owner->projected=true; result=xrtValueRetain(value); }
        }
    }
    bool ok=finish(&scope,owner,result!=NULL);
    /* A refused binding can retire the consumed input's last native owner.
     * Keep that cleanup inside an explicit first-failure boundary. */
    xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,!ok)) {
        xrtValueRelease(value); ok=scopeFinish(&cleanup,ok);
    } else { discard(value); ok=false; }
    if (!ok) { discard(result); return NULL; }
    return result;
}

/* Only a matched resident producer authorizes payload inspection. BUSY and
 * unbound native owners are failures, never a reason to fall back to wire. */
static int erasedBegin(const xvalue* value,SetOwner** output,SetOwner* already)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int status=0; SetOwner* owner=NULL; bool unbound=false;
    xvaluetype tag=value ? xrtValueType(value) : XVALUE_NULL;
    if (value && tag==XVALUE_INVALID) status=-1;
    else if (tag==XVALUE_HANDLE) {
        ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
        if (!xrtValueGetHandle(value,&raw,&ops,&user)) status=-1;
        else if (ops==&handleOps) {
            status=-1;
            if (!raw || user) xrtSetErrorKind(XERR_STATE);
            else {
                SetOwner* candidate=raw;
                if (candidate->cursorIdentity) xrtSetErrorKind(XERR_TYPE);
                else if (xrtValueTypeId(value)!=candidate->layout.identity) xrtSetErrorKind(XERR_STATE);
                else if (candidate==already) { owner=candidate; status=1; }
                else if (pin(candidate,0)) {
                    owner=candidate; unbound=!owner->projected; if (!unbound) status=1;
                }
            }
        }
    }
    mutationEnd(&mutation);
    if (unbound) { end(owner); xrtSetErrorKind(XERR_STATE); }
    if (status==1) *output=owner;
    return status;
}
static SetOwner* erased(const xvalue* value)
{
    SetOwner* owner=NULL; if (!erasedBegin(value,&owner,NULL)) xrtSetErrorKind(XERR_TYPE);
    return owner;
}
int xl6SetErasedProbeV1(const xvalue* value)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation); int result=0;
    xvaluetype tag=value ? xrtValueType(value) : XVALUE_NULL;
    if (value && tag==XVALUE_INVALID) result=-1;
    else if (tag==XVALUE_HANDLE) {
        ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
        result=!xrtValueGetHandle(value,&raw,&ops,&user) ? -1 : ops==&handleOps ? 1 : 0;
    }
    mutationEnd(&mutation); return result;
}
static bool admitsValue(const SetOwner* owner,const xvalue* value)
{
    if (value && owner->policy.ops->Value(owner->policy.data,value)) return true;
    if (!xrtGetError()) xrtSetErrorKind(value ? XERR_TYPE : XERR_ARGUMENT);
    return false;
}
/* One synchronous checked adapter protects XRT's payload-only converter
 * contract. It may not publish a slot/value after an absent first failure,
 * even when the underlying callback returns true/a seemingly valid Value. */
static bool decode(const xvalue* value,const xrttype* type,ptr output,ptr context)
{
    const SetOwner* owner=context; xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) { callbackBeginFailure(); return false; }
    bool ok=owner->projection->ToTyped(value,type,output,NULL) && slotAdmits(owner,output);
    if (!scopeFinish(&scope,ok)) { callbackFailure(); return false; }
    return true;
}
static xvalue* admitProjection(const SetOwner*,xvalue*,xl6_error_scope*);
static xvalue* encode(const xrttype* type,const void* slot,ptr context)
{
    const SetOwner* owner=context; xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) { callbackBeginFailure(); return NULL; }
    xvalue* result=owner->projection->FromTyped(type,slot,NULL);
    /* TakeValue commits removal only after this callback returns. Reject a
     * wrong logical projection here, not after XRT has removed the key. */
    result=admitProjection(owner,result,&scope);
    if (!scopeFinish(&scope,result!=NULL)) { callbackFailure(); discard(result); return NULL; }
    return result;
}
static const xvalueconverter* adapter(SetOwner* owner,xvalueconverter* codec)
{
    if (!owner->projection) return NULL;
    *codec=(xvalueconverter){.ToTyped=decode,.FromTyped=encode,.Context=owner}; return codec;
}
static xvalue* admitProjection(const SetOwner* owner,xvalue* result,xl6_error_scope* scope)
{
    if (result) {
        uint64_t identity=owner->policy.boxedIdentity; xvaluetype tag=xrtValueType(result);
        if ((identity && tag!=XVALUE_NULL && tag!=XVALUE_BOOL && !xl6ScalarIdentityBindV1(result,identity,owner->policy.scalarDomain)) ||
            !admitsValue(owner,result)) xl6ErrorScopeCaptureFailure(scope);
    }
    if (!result || xrtGetError() || scope->failed) { xl6ErrorScopeCaptureFailure(scope); discard(result); return NULL; }
    return result;
}
static xvalue* projectedSlot(SetOwner* owner,const void* slot)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    xvalueconverter codec={0}; xvalue* result=xrtValueFromTyped(owner->storage->ItemType,slot,adapter(owner,&codec));
    result=admitProjection(owner,result,&scope);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
int xl6SetGraphBeginV1(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,XL6_SetGraphViewV1* view)
{
    if (!view || view->owner) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    if (ops!=&handleOps) return 0;
    if (!raw || user) { xrtSetErrorKind(XERR_STATE); return -1; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    SetOwner* owner=raw; bool success=false,rejected=false;
    if (owner->cursorIdentity || identity!=owner->layout.identity) xrtSetErrorKind(XERR_STATE);
    else if (pin(owner,0)) {
        if (!owner->projected) rejected=true;
        else { *view=(XL6_SetGraphViewV1){owner,owner->layout,owner->storage}; success=true; }
    }
    mutationEnd(&mutation);
    if (rejected) { end(owner); xrtSetErrorKind(XERR_STATE); }
    return success ? 1 : -1;
}
void xl6SetGraphEndV1(XL6_SetGraphViewV1* view)
{
    if (!view || !view->owner) abort();
    SetOwner* owner=view->owner; *view=(XL6_SetGraphViewV1){0}; end(owner);
}
static SetOwner* graphOwner(const XL6_SetGraphViewV1* view)
{
    SetOwner* owner=view ? view->owner : NULL;
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE) || owner->cleared ||
        !owner->projected || owner->storage!=view->storage || !sameLayout(&owner->layout,&view->layout)) {
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    return owner;
}
xvalue* xl6SetGraphEmptyV1(const XL6_SetGraphViewV1* view)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    SetOwner* owner=graphOwner(view); XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (owner && owner->policy.ops->Retain(owner->policy.data)) {
        policy=owner->policy;
        const xrttype* type=xl6SequenceStorageTypeV1(owner->layout.value.transfer,owner->layout.value.type);
        const xtypedsetkeypolicy* keys=owner->layout.value.transfer==3 ? xl6SetReferenceKeyPolicyV1() :
            owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1 ? xl6SetFutureKeyPolicyV1() :
            owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 ? xl6SequenceNativeValuePolicyV1(&policy)->content : NULL;
        xtypedset* storage=keys ? xrtTypedSetCreateWithKeyPolicy(type,keys) : xrtTypedSetCreate(type);
        if (storage) result=wrap(storage,&owner->layout,&policy,0,false,owner->projection,true);
    }
    xl6SequenceAdmissionDropV3(&policy);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6SetGraphProjectSlotV1(const XL6_SetGraphViewV1* view,const void* slot)
{
    SetOwner* owner=graphOwner(view);
    if (!owner) return NULL;
    if (owner->layout.value.transfer==3 || !slot) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    return projectedSlot(owner,slot);
}
bool xl6SetErasedKeyV1(const xvalue* value,const xvalue* item,XL6_SetKeyActionV1 action,bool* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=erased(value); bool ok=false,result=false;
    if (!output || action<XL6_SET_ADD_V1 || action>XL6_SET_REMOVE_V1) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && admitsValue(owner,item) && !scope.failed && !xrtGetError()) {
        xvalueconverter codec={0}; const xvalueconverter* selected=adapter(owner,&codec); ok=true;
        if (action==XL6_SET_ADD_V1) {
            /* The private operation pin excludes intervening mutation. This
             * O(1) cardinality delta obtains 'new' without decoding twice;
             * the XRT AddValue API already reports checked operation success. */
            size_t before=xrtTypedSetCount(owner->storage);
            ok=xrtTypedSetAddValue(owner->storage,item,selected);
            if (ok && !xrtGetError() && !scope.failed) result=xrtTypedSetCount(owner->storage)>before;
        } else if (action==XL6_SET_HAS_V1) result=xrtTypedSetHasValue(owner->storage,item,selected);
        else result=xrtTypedSetRemoveValue(owner->storage,item,selected);
    }
    ok=finish(&scope,owner,ok); if (ok) *output=result; return ok;
}
bool xl6SetErasedReadV1(const xvalue* value,const xvalue* item,bool take,xvalue** output,bool* outputFound)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=erased(value); xvalue* result=NULL; bool ok=false,found=false;
    if (!output || *output || !outputFound) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && admitsValue(owner,item) && !scope.failed && !xrtGetError()) {
        xvalueconverter codec={0}; const xvalueconverter* selected=adapter(owner,&codec);
        result=take ? xrtTypedSetTakeValue(owner->storage,item,selected) : xrtTypedSetGetValue(owner->storage,item,selected);
        found=result!=NULL; ok=!xrtGetError() && !scope.failed;
        if (ok && found) { result=admitProjection(owner,result,&scope); ok=result!=NULL; }
    }
    if (!ok || xrtGetError() || scope.failed) { xl6ErrorScopeCaptureFailure(&scope); discard(result); result=NULL; }
    if (!finish(&scope,owner,ok)) { discard(result); return false; }
    *output=result; *outputFound=found; return true;
}
bool xl6SetErasedCountV1(const xvalue* value,bool capacity,int64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=erased(value); size_t count=owner ? (capacity ? xrtTypedSetCapacity(owner->storage) : xrtTypedSetCount(owner->storage)) : 0;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT); else if (count>INT64_MAX) xrtSetErrorKind(XERR_RANGE);
    bool ok=finish(&scope,owner,owner && output && count<=INT64_MAX); if (ok) *output=(int64_t)count; return ok;
}
bool xl6SetErasedClearV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=erased(value); return finish(&scope,owner,owner && xrtTypedSetClear(owner->storage));
}
bool xl6SetErasedReserveV1(const xvalue* value,int64_t count)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=erased(value); bool valid=count>=0 && (uint64_t)count<=SIZE_MAX;
    if (!valid) xrtSetErrorKind(XERR_RANGE);
    return finish(&scope,owner,owner && valid && xrtTypedSetReserve(owner->storage,(size_t)count));
}
bool xl6SetErasedTrimV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=erased(value); return finish(&scope,owner,owner && xrtTypedSetTrim(owner->storage));
}
xvalue* xl6SetErasedCloneV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    SetOwner* owner=erased(value); xvalue* result=clonePinned(owner,0,false,&scope);
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return NULL; }
    return result;
}
bool xl6SetErasedHashV1(const xvalue* value,uint64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=erased(value); uint64_t hash=0;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    bool ok=finish(&scope,owner,owner && output && hashPinned(owner,&hash)); if (ok) *output=hash; return ok;
}
bool xl6SetErasedRelationV1(const xvalue* left,const xvalue* right,XL6_SetRelationV1 relation,bool proper,bool* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* a=erased(left),*b=NULL; bool ok=false,result=false;
    if (a) (void)erasedBegin(right,&b,a);
    if (!output || relation<XL6_SET_EQUALS_V1 || relation>XL6_SET_DISJOINT_V1 ||
        (proper && (relation==XL6_SET_EQUALS_V1 || relation==XL6_SET_DISJOINT_V1))) xrtSetErrorKind(XERR_ARGUMENT);
    else if (a && b && compatible(a,b)) {
        ok=true;
        switch (relation) {
        case XL6_SET_EQUALS_V1: ok=equalPinned(a,b,&result); break;
        case XL6_SET_SUBSET_V1: result=xrtTypedSetIsSubset(a->storage,b->storage,proper); break;
        case XL6_SET_SUPERSET_V1: result=xrtTypedSetIsSuperset(a->storage,b->storage,proper); break;
        case XL6_SET_DISJOINT_V1: result=xrtTypedSetIsDisjoint(a->storage,b->storage); break;
        }
    }
    ok=finishPair(&scope,a,b,ok); if (ok) *output=result; return ok;
}
bool xl6SetErasedMergeV1(const xvalue* target,const xvalue* source)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* a=erased(target),*b=NULL; if (a) (void)erasedBegin(source,&b,a);
    return finishPair(&scope,a,b,a && b && compatible(a,b) && xrtTypedSetMerge(a->storage,b->storage));
}
xvalue* xl6SetErasedAlgebraV1(const xvalue* left,const xvalue* right,XL6_SetAlgebraV1 operation)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    SetOwner* a=erased(left),*b=NULL; if (a) (void)erasedBegin(right,&b,a);
    xvalue* result=algebraPinned(a,b,operation,&scope);
    if (!finishPair(&scope,a,b,result!=NULL)) { discard(result); return NULL; }
    return result;
}
int xl6SetIteratorSnapshotErasedV1(const xvalue* value,bool reverse,xvalue** output,XL6_SetLayoutV1* outputLayout,uint64_t* outputIdentity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return -1;
    SetOwner* owner=NULL; int status=erasedBegin(value,&owner,NULL);
    if (status!=1) return finish(&scope,NULL,status==0) && status==0 ? 0 : -1;
    XL6_SetLayoutV1 layout=owner->layout; uint64_t identity=layout.identity;
    do { ++identity; } while (!identity || identity==layout.identity || identity==layout.value.identity);
    xvalue* result=NULL;
    if (!output || *output || !outputLayout || !outputIdentity) xrtSetErrorKind(XERR_ARGUMENT);
    else result=clonePinned(owner,identity,reverse,&scope);
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return -1; }
    *output=result; *outputLayout=layout; *outputIdentity=identity; return 1;
}
bool xl6SetIteratorAdvanceValueV1(const xvalue* value,const XL6_SetLayoutV1* layout,uint64_t identity,bool* outputFound,xvalue** output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    SetOwner* owner=identity ? begin(value,layout,identity) : NULL; bool ok=false,found=false; xvalue* result=NULL;
    if (!identity || !outputFound || !output || *output) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && !owner->projected) xrtSetErrorKind(XERR_STATE);
    else if (owner) {
        ok=cursorAdvancePinned(owner,&found);
        if (ok && found) { result=projectedSlot(owner,owner->current); ok=result!=NULL; }
    }
    if (owner && (!ok || xrtGetError() || scope.failed)) { owner->failed=true; cursorEnd(owner); }
    if (!finish(&scope,owner,ok)) { discard(result); return false; }
    *outputFound=found; *output=result; return true;
}
