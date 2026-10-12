#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_dict_graph_access.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct DictOwner {
    volatile int32_t refs;
    xatomic32 busy;
    XL6_DictLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy;
    const xvalueconverter* projection;
    bool projected;
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
    else if (empty && owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1)
        empty=xl6SequenceNativeValuePolicyV1(&owner->policy)->Empty(output);
    if (!empty) xrtSetErrorKind(XERR_ARGUMENT);
    return empty;
}
static bool overlaps(const void* a,size_t as,const void* b,size_t bs)
{ uintptr_t left=(uintptr_t)a,right=(uintptr_t)b; return left<=right ? right-left<as : left-right<bs; }

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
    else if (owner->layout.value.transfer>=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1)
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
        if (owner->layout.value.transfer>=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) {
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
    XL6_SequenceAdmissionV3* policy,uint64_t cursor,bool reverse,
    const xvalueconverter* projection,bool projected)
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
        .cursorIdentity=cursor,.reverse=reverse,.projection=projection,.projected=projected};
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
        if (storage) result=wrap(storage,layout,&policy,0,false,NULL,false);
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
                owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 ?
                    xl6SequenceNativeValuePolicyV1(&owner->policy)->Copy(slot,output) :
                    xrtTypeCopyValue(xrtTypedDictItemType(owner->storage),output,slot);
            result=success;
        }
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
bool xl6DictReadSlotV1(const xvalue* value,const XL6_DictLayoutV1* layout,xstrview key,
    void* output,bool required)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    DictOwner* owner=begin(value,layout,0); bool success=false,copied=false;
    const xrttype* type=NULL; size_t size=0;
    if (owner && outputEmpty(owner,output)) {
        /* The resident storage descriptor, not caller metadata, owns rollback.
         * It is context-free and remains valid after the operation pin ends. */
        type=xrtTypedDictItemType(owner->storage); size=owner->layout.value.size;
        const void* slot=xrtTypedDictConstGet(owner->storage,key);
        if (slot && owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1)
            success=copied=xl6SequenceNativeValuePolicyV1(&owner->policy)->Copy(slot,output);
        else if (slot) success=copied=xrtTypeCopyValue(type,output,slot);
        else if (!xrtGetError()) {
            if (required) xrtSetErrorKind(XERR_RANGE);
            else if (owner->layout.value.transfer==3) {
                xvalue* empty=xrtValueRetain(xrtValueNull());
                if (empty) { *(xvalue**)output=empty; success=copied=true; }
            } else success=true;
        }
    }
    success=finish(&scope,owner,success);
    if (!success && copied) {
        xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
        if (xl6ErrorScopeBeginCleanup(&cleanup,false)) {
            xrtErrorFree(incoming); xrtTypeDropValue(type,output); memset(output,0,size);
            (void)scopeFinish(&cleanup,true);
        } else {
            xerror* prior=incoming ? incoming : xrtTakeError(); xrtClearError();
            xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)});
            xrtClearError(); xrtTypeDropValue(type,output); memset(output,0,size);
            xrtClearError(); xrtSetErrorTake(prior);
        }
    }
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
        if (storage) result=wrap(storage,&owner->layout,&policy,cursor,reverse,owner->projection,owner->projected);
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
static bool cursorAdvancePinned(DictOwner* owner,bool* found,xl6_error_scope* scope)
{
    bool success=false,result=false;
    if (owner->failed) xrtSetErrorKind(XERR_STATE);
    else if (owner->ended) success=true;
    else {
        if (!owner->started) owner->started=owner->reverse ?
            xrtTypedDictIterRBegin(owner->storage,&owner->iterator) :
            xrtTypedDictIterBegin(owner->storage,&owner->iterator);
        if (owner->started) {
            owner->current=xrtTypedDictIterNext(&owner->iterator,&owner->currentKey);
            success=!xrtGetError() && !scope->failed; result=success && owner->current;
        }
        if (!success) owner->failed=true;
        if (!result) cursorEnd(owner);
    }
    if (success) *found=result;
    return success;
}
bool xl6DictCursorAdvanceV1(const xvalue* value,const XL6_DictLayoutV1* layout,uint64_t identity,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=identity ? begin(value,layout,identity) : NULL; bool success=false,result=false;
    if (!identity || !found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) success=cursorAdvancePinned(owner,&result,&scope);
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

xvalue* xl6DictProjectionTakeV1(xvalue* value,const XL6_DictLayoutV1* layout,const xvalueconverter* codec)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) { discard(value); return NULL; }
    DictOwner* owner=begin(value,layout,0); xvalue* result=NULL;
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
    xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,!ok)) {
        xrtValueRelease(value); ok=scopeFinish(&cleanup,ok);
    } else { discard(value); ok=false; }
    if (!ok) { discard(result); return NULL; }
    return result;
}
/* Matching the private producer is mandatory before dereferencing its payload.
 * A genuine but unbound/busy/cursor/cleared owner must never become wire data. */
static int erasedBegin(const xvalue* value,DictOwner** output,DictOwner* already)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int status=0; DictOwner* owner=NULL; bool unbound=false;
    xvaluetype tag=value ? xrtValueType(value) : XVALUE_NULL;
    if (value && tag==XVALUE_INVALID) status=-1;
    else if (tag==XVALUE_HANDLE) {
        ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL;
        if (!xrtValueGetHandle(value,&raw,&ops,&user)) status=-1;
        else if (ops==&handleOps) {
            status=-1;
            if (!raw || user) xrtSetErrorKind(XERR_STATE);
            else {
                DictOwner* candidate=raw;
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
static DictOwner* erased(const xvalue* value)
{
    DictOwner* owner=NULL; if (!erasedBegin(value,&owner,NULL)) xrtSetErrorKind(XERR_TYPE);
    return owner;
}
int xl6DictErasedProbeV1(const xvalue* value)
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
static bool admitsValue(const DictOwner* owner,const xvalue* value)
{
    if (value && owner->policy.ops->Value(owner->policy.data,value)) return true;
    if (!xrtGetError()) xrtSetErrorKind(value ? XERR_TYPE : XERR_ARGUMENT);
    return false;
}
static void callbackFailure(void) { if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); }
static void callbackBeginFailure(void)
{ xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); callbackFailure(); }
/* Resident converters are invoked synchronously in a checked boundary.
 * Check BOTH logical admission and the physical slot, including an absent
 * first failure behind a true callback. No transient adapter is retained. */
static bool decode(DictOwner* owner,const xvalue* value,const xrttype* type,ptr output,bool* initialized)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) { callbackBeginFailure(); return false; }
    bool ok;
    if (owner->projection) {
        *initialized=xrtTypeInitValue(type,output);
        ok=*initialized && owner->projection->ToTyped(value,type,output,NULL);
    } else {
        ok=xrtValueToTyped(value,type,output,NULL); *initialized=ok;
        /* XRT retires a failed ordinary conversion itself. */
    }
    ok=ok && slotAdmits(owner,output);
    if (!scopeFinish(&scope,ok)) { callbackFailure(); return false; }
    return true;
}
static xvalue* admitProjection(const DictOwner* owner,xvalue* result,xl6_error_scope* scope)
{
    if (result) {
        uint64_t identity=owner->policy.boxedIdentity; xvaluetype tag=xrtValueType(result);
        /* Retained reference slots already carry their authentic identity.
         * Never rebind a borrowed source child while projecting a container. */
        if ((owner->layout.value.transfer!=3 && identity && tag!=XVALUE_NULL && tag!=XVALUE_BOOL &&
            !xrtValueTypeIdBind(result,identity)) || !admitsValue(owner,result))
            xl6ErrorScopeCaptureFailure(scope);
    }
    if (!result || xrtGetError() || scope->failed) { xl6ErrorScopeCaptureFailure(scope); discard(result); return NULL; }
    return result;
}
static xvalue* encode(DictOwner* owner,const xrttype* type,const void* slot)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) { callbackBeginFailure(); return NULL; }
    xvalue* raw=owner->projection ? owner->projection->FromTyped(type,slot,NULL) : xrtValueFromTyped(type,slot,NULL);
    xvalue* result=admitProjection(owner,raw,&scope);
    if (!scopeFinish(&scope,result!=NULL)) { callbackFailure(); discard(result); return NULL; }
    return result;
}
static bool writeValue(DictOwner* owner,xstrview key,const xvalue* item,xl6_error_scope* scope)
{
    if (!admitsValue(owner,item) || xrtGetError() || scope->failed) return false;
    /* Public XRT Value converters are fallbacks, not overrides for builtins.
     * Invoke the declared resident policy explicitly before any storage commit.
     * Canonical language carriers fit the aligned inline slot. Large/overaligned
     * resident inline types use the same overflow-checked aligned allocation
     * rule as XRT's typed-value scratch, never a generated callback/descriptor. */
    union { max_align_t align; unsigned char bytes[64]; } local={0};
    const xrttype* type=owner->storage->ItemType; ptr allocation=NULL,slot=local.bytes;
    if (type->Size>sizeof(local.bytes) || type->Align>_Alignof(max_align_t)) {
        if (!type->Align || type->Size>SIZE_MAX-(type->Align-1)) { xrtSetErrorKind(XERR_RANGE); return false; }
        allocation=xrtMalloc(type->Size+type->Align-1);
        if (!allocation) return false;
        slot=(ptr)(((uintptr_t)allocation+type->Align-1)&~((uintptr_t)type->Align-1));
    }
    bool initialized=false,ok=decode(owner,item,type,slot,&initialized);
    if (ok) ok=xrtTypedDictSet(owner->storage,key,slot);
    if (!ok || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    if (initialized) xrtTypeDropValue(type,slot);
    xrtFree(allocation); return ok;
}
static xvalue* projectedSlot(DictOwner* owner,const void* slot)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    xvalue* result=encode(owner,owner->storage->ItemType,slot);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
bool xl6DictErasedSetV1(const xvalue* value,xstrview key,const xvalue* item)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=erased(value);
    return finish(&scope,owner,owner && writeValue(owner,key,item,&scope));
}
bool xl6DictErasedReadV1(const xvalue* value,xstrview key,bool take,xvalue** output,bool* outputFound)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=erased(value); xvalue* result=NULL; bool ok=false,found=false;
    if (!output || *output || !outputFound || overlaps(output,sizeof(*output),outputFound,sizeof(*outputFound)))
        xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        const void* slot=xrtTypedDictConstGet(owner->storage,key); ok=!xrtGetError() && !scope.failed;
        if (ok && slot) {
            result=projectedSlot(owner,slot); found=result!=NULL; ok=found;
            /* Admission/projection must finish BEFORE a take removes the key. */
            if (ok && take) ok=xrtTypedDictRemove(owner->storage,key);
        }
    }
    if (!finish(&scope,owner,ok)) { discard(result); return false; }
    *output=result; *outputFound=found; return true;
}
bool xl6DictErasedKeyV1(const xvalue* value,xstrview key,bool remove,bool* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=erased(value); bool ok=false,result=false;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        result=xrtTypedDictHas(owner->storage,key);
        ok=!xrtGetError() && !scope.failed && (!result || !remove || xrtTypedDictRemove(owner->storage,key));
    }
    if (!finish(&scope,owner,ok)) return false;
    *output=result; return true;
}
bool xl6DictValueFinishReadV1(xvalue* value,bool success)
{
    if (!success) {
        xerror* primary=xrtTakeError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(primary)});
        xrtSetErrorTake(primary);
    }
    xl6_error_scope cleanup={0};
    if (!xl6ErrorScopeBeginCleanup(&cleanup,!success)) {
        xerror* primary=xrtTakeError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(primary)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(primary);
        return false;
    }
    xrtValueRelease(value);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return success && outcome.ok;
}
bool xl6DictErasedVisitKeysV1(const xvalue* value,bool (*visit)(xstrview,void*),void* context)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=erased(value); bool ok=false;
    if (!visit) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        xtypeddictiter cursor={0};
        ok=xrtTypedDictIterBegin(owner->storage,&cursor);
        if (ok) {
            xstrview key={0}; const void* item;
            while ((item=xrtTypedDictIterNext(&cursor,&key))!=NULL) {
                (void)item;
                if (!visit(key,context)) { ok=false; break; }
            }
            if (xrtGetError() || scope.failed) ok=false;
            xrtTypedDictIterEnd(&cursor);
        }
    }
    return finish(&scope,owner,ok);
}
bool xl6DictErasedCountV1(const xvalue* value,bool capacity,int64_t* output)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=erased(value); size_t count=owner ?
        (capacity ? xrtTypedDictCapacity(owner->storage) : xrtTypedDictCount(owner->storage)) : 0;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT); else if (count>INT64_MAX) xrtSetErrorKind(XERR_RANGE);
    if (!finish(&scope,owner,owner && output && count<=INT64_MAX)) return false;
    *output=(int64_t)count; return true;
}
bool xl6DictErasedClearV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=erased(value); return finish(&scope,owner,owner && xrtTypedDictClear(owner->storage));
}
bool xl6DictErasedReserveV1(const xvalue* value,int64_t count)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=erased(value); bool valid=count>=0 && (uint64_t)count<=SIZE_MAX;
    if (!valid) xrtSetErrorKind(XERR_RANGE);
    return finish(&scope,owner,owner && valid && xrtTypedDictReserve(owner->storage,(size_t)count));
}
bool xl6DictErasedTrimV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=erased(value); return finish(&scope,owner,owner && xrtTypedDictTrim(owner->storage));
}
xvalue* xl6DictErasedCloneV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    DictOwner* owner=erased(value); xvalue* result=owner ? clonePinned(owner,0,false) : NULL;
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return NULL; }
    return result;
}
int xl6DictIteratorSnapshotErasedV1(const xvalue* value,bool reverse,xvalue** output,
    XL6_DictLayoutV1* outputLayout,uint64_t* outputIdentity)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return -1;
    DictOwner* owner=NULL; int status=erasedBegin(value,&owner,NULL);
    if (status!=1) return finish(&scope,NULL,status==0) && status==0 ? 0 : -1;
    XL6_DictLayoutV1 layout=owner->layout; uint64_t identity=layout.identity;
    do { ++identity; } while (!identity || identity==layout.identity || identity==layout.value.identity);
    xvalue* result=NULL;
    if (!output || *output || !outputLayout || !outputIdentity) xrtSetErrorKind(XERR_ARGUMENT);
    else result=clonePinned(owner,identity,reverse);
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return -1; }
    *output=result; *outputLayout=layout; *outputIdentity=identity; return 1;
}
static bool advanceValue(const xvalue* value,const XL6_DictLayoutV1* layout,
    uint64_t identity,bool* outputFound,xvalue** output,xstrview* outputKey,bool ownedKey)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    DictOwner* owner=identity ? begin(value,layout,identity) : NULL;
    bool ok=false,found=false,advanced=false; xvalue* result=NULL; xstrview key={0};
    if (!identity || !outputFound || !output || *output || !outputKey || outputKey->Data || outputKey->Size)
        xrtSetErrorKind(XERR_ARGUMENT);
    else if (overlaps(output,sizeof(*output),outputKey,sizeof(*outputKey)) ||
        overlaps(output,sizeof(*output),outputFound,sizeof(*outputFound)) ||
        overlaps(outputKey,sizeof(*outputKey),outputFound,sizeof(*outputFound))) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && !owner->projected) xrtSetErrorKind(XERR_STATE);
    else if (owner) {
        advanced=true; ok=cursorAdvancePinned(owner,&found,&scope);
        if (ok && found) {
            result=projectedSlot(owner,owner->current); ok=result!=NULL;
            if (ok) {
                key=owner->currentKey;
                if (ownedKey) { key.Data=xrtStrDupView(key); ok=key.Data!=NULL; }
            }
        }
    }
    if (owner && advanced && (!ok || xrtGetError() || scope.failed)) { owner->failed=true; cursorEnd(owner); }
    if (!finish(&scope,owner,ok)) { discard(result); if (ownedKey) xrtFree((ptr)key.Data); return false; }
    *output=result; *outputFound=found; *outputKey=key; return true;
}
bool xl6DictIteratorAdvanceValueV1(const xvalue* value,const XL6_DictLayoutV1* layout,
    uint64_t identity,bool* found,xvalue** item,xstrview* key)
{ return advanceValue(value,layout,identity,found,item,key,true); }
bool xl6DictIteratorAdvanceViewV1(const xvalue* value,const XL6_DictLayoutV1* layout,
    uint64_t identity,bool* found,xvalue** item,xstrview* key)
{ return advanceValue(value,layout,identity,found,item,key,false); }

int xl6DictGraphBeginV1(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,XL6_DictGraphViewV1* view)
{
    if (!view || view->owner) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    if (ops!=&handleOps) return 0;
    if (!raw || user) { xrtSetErrorKind(XERR_STATE); return -1; }
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    DictOwner* owner=raw; bool success=false,rejected=false;
    if (owner->cursorIdentity || identity!=owner->layout.identity) xrtSetErrorKind(XERR_STATE);
    else if (pin(owner,0)) {
        if (!owner->projected) rejected=true;
        else { *view=(XL6_DictGraphViewV1){owner,owner->layout,owner->storage}; success=true; }
    }
    mutationEnd(&mutation);
    if (rejected) { end(owner); xrtSetErrorKind(XERR_STATE); }
    return success ? 1 : -1;
}
void xl6DictGraphEndV1(XL6_DictGraphViewV1* view)
{
    if (!view || !view->owner) abort();
    DictOwner* owner=view->owner; *view=(XL6_DictGraphViewV1){0}; end(owner);
}
static DictOwner* graphOwner(const XL6_DictGraphViewV1* view)
{
    DictOwner* owner=view ? view->owner : NULL;
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE) || owner->cleared ||
        !owner->projected || owner->storage!=view->storage || !sameLayout(&owner->layout,&view->layout)) {
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    return owner;
}
xvalue* xl6DictGraphEmptyV1(const XL6_DictGraphViewV1* view)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    DictOwner* owner=graphOwner(view); XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (owner && owner->policy.ops->Retain(owner->policy.data)) {
        policy=owner->policy;
        xtypeddict* storage=xrtTypedDictCreate(owner->storage->ItemType);
        if (storage) result=wrap(storage,&owner->layout,&policy,0,false,owner->projection,true);
    }
    xl6SequenceAdmissionDropV3(&policy);
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6DictGraphProjectSlotV1(const XL6_DictGraphViewV1* view,const void* slot)
{
    DictOwner* owner=graphOwner(view); if (!owner) return NULL;
    if (owner->layout.value.transfer==3 || !slot) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    return projectedSlot(owner,slot);
}
static xvalue* wirePinned(DictOwner* owner)
{
    xvalue* result=NULL; xtypeddictiter iterator={0};
    bool started=false,ok=false; xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) { discard(result); return NULL; }
    result=xrtValueObject();
    if (!result || !xrtValueReserve(result,xrtTypedDictCount(owner->storage))) goto done;
    if (!xrtTypedDictIterBegin(owner->storage,&iterator)) goto done;
    started=true;
    for (;;) {
        xstrview key={0}; const void* slot=xrtTypedDictIterNext(&iterator,&key);
        if (xrtGetError() || scope.failed) break;
        if (!slot) { ok=true; break; }
        xvalue* item=projectedSlot(owner,slot);
        if (!item || !xrtValueObjectSetTake(result,key,&item)) { discard(item); break; }
    }
done:
    if (!ok || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtTypedDictIterEnd(&iterator);
    if (!scopeFinish(&scope,ok)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6DictToWireV1(const xvalue* value)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    DictOwner* owner=erased(value); xvalue* result=owner ? wirePinned(owner) : NULL;
    if (!finish(&scope,owner,result!=NULL)) { discard(result); return NULL; }
    return result;
}
xvalue* xl6DictImportTakeV1(xvalue* seed,const xvalue* source)
{
    xl6_error_scope scope={0};
    if (!(seed ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        discard(seed); return NULL;
    }
    DictOwner* target=NULL; DictOwner* input=NULL; xvalue* wire=NULL;
    xvalueiter iterator={0}; bool started=false,success=false;
    if (!seed || !source || seed==source) { xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    target=erased(seed); if (!target) goto done;
    if (xrtTypedDictCount(target->storage)) { xrtSetErrorKind(XERR_STATE); goto done; }
    int native=erasedBegin(source,&input,target);
    if (native<0) goto done;
    if (input==target) { input=NULL; xrtSetErrorKind(XERR_ARGUMENT); goto done; }
    if (native) {
        if (sameLayout(&target->layout,&input->layout) && samePolicy(&target->policy,&input->policy) &&
            target->projection==input->projection) {
            success=xrtTypedDictMerge(target->storage,input->storage,false); goto done;
        }
        wire=wirePinned(input); if (!wire || xrtGetError() || scope.failed) goto done;
        end(input); input=NULL; source=wire;
    }
    if (xrtValueType(source)!=XVALUE_OBJECT) { xrtSetErrorKind(XERR_TYPE); goto done; }
    if (!xrtValueIterBegin(source,&iterator)) goto done;
    started=true;
    for (;;) {
        xvaluekey key={0}; xvalue* item=NULL;
        xvalueiterresult status=xrtValueIterAdvance(&iterator,&key,&item);
        if (status==XVALUE_ITER_END) { success=true; break; }
        if (status!=XVALUE_ITER_ITEM || xrtGetError() || scope.failed) break;
        if (key.Type!=XVALUE_KEY_STRING) { xrtSetErrorKind(XERR_STATE); break; }
        if (!writeValue(target,key.String,item,&scope)) break;
    }
done:
    if (!success || xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtValueIterEnd(&iterator);
    discard(wire);
    if (input) end(input);
    if (target) end(target);
    bool ok=scopeFinish(&scope,success);
    if (!ok) { discard(seed); return NULL; }
    return seed;
}
