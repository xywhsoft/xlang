#include "xl6_slot_map.h"
#include "xl6_slot_map_key.h"
#include "src/type/xl6_native_value_ids.h"
#include "xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>
#include <string.h>

typedef struct SlotMapOwner {
    volatile int32_t refs;
    xatomic32 busy;
    XL6_SlotMapLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy;
    XL6_SlotMapInstanceV1* instance;
    xslotmap storage,retired;
    const void* claim;
    bool cleared;
} SlotMapOwner;

static xl6_error_result scopeOutcome(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    return xl6ErrorScopeEnd(scope);
}
/* Language SlotMap operations use explicit NULL/false/status carriers. Only
 * internal void cleanup and ownership callbacks propagate out of a scope. */
static bool scopeFinish(xl6_error_scope* scope,bool success)
{
    xl6_error_result result=scopeOutcome(scope,success);
    if (!result.ok) xrtSetErrorTake(result.error);
    return result.ok;
}
static void scopePropagate(xl6_error_scope* scope,bool success)
{
    xl6_error_result result=scopeOutcome(scope,success);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
}
static bool scopeBegin(xl6_error_scope* scope)
{ return xl6ErrorScopeBegin(scope); }
static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static bool sameLayout(const XL6_SlotMapLayoutV1* a,const XL6_SlotMapLayoutV1* b)
{
    return a && b && a->identity==b->identity && a->value.identity==b->value.identity &&
        a->value.elementIdentity==b->value.elementIdentity && a->value.transfer==b->value.transfer &&
        a->value.size==b->value.size && a->value.type==b->value.type;
}
static bool capture(const XL6_SlotMapLayoutV1* layout,const xvalue* witness,XL6_SequenceAdmissionV3* output)
{
    if (!layout || !layout->identity || layout->identity==layout->value.identity || !witness) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    const XL6_SequenceLayoutV1* expected=&layout->value;
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 actual={0}; XL6_SequenceAdmissionV3 policy={0};
    bool success=false;
    if (!xl6SequenceTupleV1(expected->identity,expected->elementIdentity,expected->transfer,expected->size,expected->type) ||
        !xl6SequenceSchemaCopyV1(witness,&role,&actual,&policy)) goto done;
    if (role!=XL6_SEQUENCE_ARRAY_V1 || actual.identity!=expected->identity ||
        actual.elementIdentity!=expected->elementIdentity || actual.transfer!=expected->transfer ||
        actual.size!=expected->size || actual.type!=expected->type ||
        expected->type->Align>_Alignof(max_align_t) ||
        (expected->transfer==4 && policy.ops!=xl6ArrayFutureAdmissionOpsV2Get())) {
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
static const xrttype* storageType(const SlotMapOwner* owner)
{ return xl6SequenceStorageTypeV1(owner->layout.value.transfer,owner->layout.value.type); }
/* Cleanup continues through every element and retains the first failure. */
static void storageDestroy(xslotmap* storage,const xrttype* type,xl6_error_scope* scope)
{
    if (!storage->Version) return;
    xslotmapiter iterator={0};
    if (xrtSlotMapIterBegin(storage,&iterator)) {
        ptr item; while ((item=xrtSlotMapIterNext(&iterator,NULL))) {
            xrtTypeDropValue(type,item); xrtFree(item);
            if (scope && (xrtGetError() || scope->failed)) xl6ErrorScopeCaptureFailure(scope);
        }
        xrtSlotMapIterEnd(&iterator);
    }
    xrtSlotMapUnit(storage);
}
static void discard(xvalue* value)
{
    if (!value) return;
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    if (xl6ErrorScopeBeginCleanup(&cleanup,false)) {
        xrtErrorFree(incoming); xrtValueRelease(value); scopePropagate(&cleanup,true);
    } else {
        xerror* prior=incoming ? incoming : xrtTakeError(); xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)});
        xrtClearError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(prior);
    }
}
static void drop(ptr opaque,ptr context)
{
    (void)context; SlotMapOwner* owner=opaque; if (!owner) return;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int32_t refs=xrtRefRelease(&owner->refs); if (refs<0) abort();
    if (refs) { mutationEnd(&mutation); return; }
    if (owner->retired.Version || xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    mutationEnd(&mutation);
    xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
    bool entered=xl6ErrorScopeBeginCleanup(&cleanup,false); xerror* prior=NULL;
    if (entered) xrtErrorFree(incoming);
    else {
        prior=incoming ? incoming : xrtTakeError(); xrtClearError();
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(prior)}); xrtClearError();
    }
    storageDestroy(&owner->storage,storageType(owner),entered ? &cleanup : NULL);
    xl6SequenceAdmissionDropV3(&owner->policy); xl6SlotMapInstanceDropV1(owner->instance); xrtFree(owner);
    if (entered) scopePropagate(&cleanup,true);
    else { xrtClearError(); xrtSetErrorTake(prior); }
}
static bool retain(ptr opaque,ptr* output,ptr context)
{
    (void)context; SlotMapOwner* owner=opaque; xrtownershipscope mutation={0}; mutationBegin(&mutation);
    bool success=owner && output && !owner->cleared && xrtRefRetain(&owner->refs)>0;
    if (success) *output=owner; else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return success;
}
static uint64 unsupportedHash(ptr owner,ptr context)
{ (void)owner; (void)context; xrtSetErrorKind(XERR_UNSUPPORTED); return 0; }
static bool unsupportedEqual(ptr a,ptr b,ptr context)
{ (void)a; (void)b; (void)context; xrtSetErrorKind(XERR_UNSUPPORTED); return false; }
static const xvaluehandleops handleOps={retain,drop,unsupportedHash,unsupportedEqual};
bool xl6SlotMapAdmitsSchemaV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,
    const XL6_SequenceAdmissionV3* policy)
{
    if(!value || !layout || !policy || !policy->ops || !policy->data ||
        xrtValueType(value)!=XVALUE_HANDLE || xrtValueTypeId(value)!=layout->identity)return false;
    xrtownershipscope scope={0}; mutationBegin(&scope);
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL; bool admitted=false;
    if(xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        const SlotMapOwner* owner=raw; const XL6_SequenceAdmissionV3* domain=&owner->policy;
        admitted=!owner->cleared && sameLayout(&owner->layout,layout) && domain->ops==policy->ops &&
            domain->elementIdentity==policy->elementIdentity && domain->transfer==policy->transfer &&
            domain->size==policy->size && domain->type==policy->type && domain->boxedIdentity==policy->boxedIdentity &&
            domain->scalarDomain==policy->scalarDomain &&
            (domain->data==policy->data || domain->ops->Equivalent(domain->data,policy->data));
    }
    mutationEnd(&scope); return admitted;
}
static SlotMapOwner* begin(const xvalue* value,const XL6_SlotMapLayoutV1* layout)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL; SlotMapOwner* owner=NULL;
    if (value && layout && layout->identity && xrtValueTypeId(value)==layout->identity &&
        xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        SlotMapOwner* candidate=raw; uint32 available=0;
        if (!candidate->cleared && sameLayout(&candidate->layout,layout) &&
            xrtAtomic32CompareExchange(&candidate->busy,&available,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
            if (xrtRefRetain(&candidate->refs)>0) owner=candidate;
            else xrtAtomic32Store(&candidate->busy,0,XMEMORY_RELEASE);
        }
    }
    if (!owner) xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return owner;
}
static void end(SlotMapOwner* owner)
{
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE); mutationEnd(&mutation); drop(owner,NULL);
}
static bool finish(xl6_error_scope* scope,SlotMapOwner* owner,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    if (owner) end(owner);
    return scopeFinish(scope,true);
}
static bool ownershipCount(const void* raw,size_t* result)
{
    const SlotMapOwner* owner=raw;
    if (!owner || !result || owner->refs<=0 || owner->cleared || xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) return false;
    *result=(size_t)owner->refs; return true;
}
static bool ownershipTrace(const void* raw,xrtownershipvisitor visit,ptr context)
{
    const SlotMapOwner* owner=raw; size_t refs; if (!visit || !ownershipCount(raw,&refs)) return false;
    if (owner->layout.value.transfer<3) return true;
    xslotmapiter iterator={0}; if (!xrtSlotMapIterBegin(&owner->storage,&iterator)) return false;
    ptr item; size_t count=0; bool success=true;
    while ((item=xrtSlotMapIterNext(&iterator,NULL))) {
        success=owner->layout.value.transfer==3 ? visit(xrtValueOwnership(*(xvalue**)item),context) :
            owner->policy.ops->TraceSlot(owner->policy.data,item,visit,context);
        if (!success) break;
        ++count;
    }
    xrtSlotMapIterEnd(&iterator); return success && count==owner->storage.Count;
}
static const xrtownershipops ownershipOps={ownershipCount,ownershipTrace};
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,ptr context)
{
    ptr raw=NULL,user=NULL; const xvaluehandleops* actual=NULL;
    return visit && xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user &&
        visit((xrtownershipref){raw,&ownershipOps},context);
}
static bool ownershipHold(const void* raw)
{ SlotMapOwner* owner=(SlotMapOwner*)raw; return owner->refs>0 && owner->refs<INT32_MAX && xrtRefRetain(&owner->refs)>0; }
static void ownershipDrop(const void* raw) { drop((ptr)raw,NULL); }
static bool ownershipClaim(const void* raw,const void* token)
{
    SlotMapOwner* owner=(SlotMapOwner*)raw;
    if (!token || owner->cleared || (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void ownershipRestore(const void* raw,const void* token)
{
    SlotMapOwner* owner=(SlotMapOwner*)raw; if (!token || owner->claim!=token || owner->cleared) abort(); owner->claim=NULL;
}
static void ownershipClear(const void* raw,const void* token)
{
    SlotMapOwner* owner=(SlotMapOwner*)raw;
    if (!token || owner->claim!=token || owner->cleared || owner->retired.Version ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    owner->retired=owner->storage; owner->storage=(xslotmap){0}; owner->cleared=true;
}
static bool ownershipFinish(const void* raw,const void* token)
{
    SlotMapOwner* owner=(SlotMapOwner*)raw; if (!token || owner->claim!=token || !owner->cleared) abort();
    xl6_error_scope cleanup={0},work={0}; if (!xl6ErrorScopeBeginCleanup(&cleanup,false)) return false;
    if (!scopeBegin(&work)) { scopePropagate(&cleanup,false); return false; }
    xslotmap retired=owner->retired; owner->retired=(xslotmap){0}; storageDestroy(&retired,storageType(owner),&work);
    xl6_error_result completed=scopeOutcome(&work,true);
    bool success=completed.ok;
    if (!success) xl6ErrorScopePropagateFailure(completed);
    scopePropagate(&cleanup,true); return success;
}
const xrtownershipadapterv1* xl6SlotMapOwnershipAdapterV2(xrtownershipref ref,
    const XL6_SequenceAdmissionOpsV2* const* allowed,size_t count)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),ownershipHold,ownershipDrop,ownershipClaim,
        ownershipRestore,NULL,ownershipClear,ownershipFinish};
    if (ref.Data && ref.Ops==&ownershipOps) {
        size_t refs; if (!ownershipCount(ref.Data,&refs)) return NULL;
        const SlotMapOwner* owner=ref.Data;
        if (owner->layout.value.transfer>=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1) {
            bool found=false; for (size_t i=0;allowed && i<count;++i) found=found || allowed[i]==owner->policy.ops;
            if (!found) return NULL;
        }
        return &adapter;
    }
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}
const xrtownershipadapterv1* xl6SlotMapOwnershipAdapterV1(xrtownershipref ref)
{ return xl6SlotMapOwnershipAdapterV2(ref,NULL,0); }
xvalue* xl6SlotMapCreateTakeV1(const XL6_SlotMapLayoutV1* layout,xvalue* witness)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) { discard(witness); return NULL; }
    XL6_SequenceAdmissionV3 policy={0}; SlotMapOwner* owner=NULL; xvalue* result=NULL;
    if (!capture(layout,witness,&policy)) goto done;
    owner=xrtCalloc(1,sizeof(*owner)); if (!owner) goto done;
    owner->refs=1; owner->layout=*layout; owner->policy=policy; policy=(XL6_SequenceAdmissionV3){0};
    xrtAtomic32Init(&owner->busy,0);
    owner->instance=xl6SlotMapInstanceCreateV1(); if (!owner->instance) goto done;
    if (!xrtSlotMapInit(&owner->storage)) goto done;
    result=xrtValueHandleTake((ptr*)&owner,&handleOps,NULL);
    if (result && (!xrtValueHandleOwnershipBindPhased(result,handleTrace) || !xrtValueTypeIdBind(result,layout->identity))) {
        xl6ErrorScopeCaptureFailure(&scope); discard(result); result=NULL;
    }
done:
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) drop(owner,NULL);
    xl6SequenceAdmissionDropV3(&policy); discard(witness);
    if (scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope); discard(result); result=NULL;
    }
    if (!scopeFinish(&scope,result!=NULL)) return NULL;
    return result;
}
static bool admits(const SlotMapOwner* owner,const void* input)
{
    if (input && owner->policy.ops->Slot(owner->policy.data,input)) return true;
    if (!xrtGetError()) xrtSetErrorKind(input ? XERR_TYPE : XERR_ARGUMENT);
    return false;
}
static ptr copyItem(const SlotMapOwner* owner,const void* input,xl6_error_scope* scope)
{
    if (!admits(owner,input)) return NULL;
    ptr item=xrtCalloc(1,owner->layout.value.size); if (!item) return NULL;
    if (!xrtTypeCopyValue(storageType(owner),item,input)) {
        xl6ErrorScopeCaptureFailure(scope);
        xrtTypeDropValue(storageType(owner),item); xrtFree(item); return NULL;
    }
    return item;
}
static bool present(const SlotMapOwner* owner,XL6_SlotMapKeyV1 key)
{ return key.instance==owner->instance && xrtSlotMapContains(&owner->storage,key.slot); }
static bool overlaps(const void* a,size_t as,const void* b,size_t bs)
{ uintptr_t left=(uintptr_t)a,right=(uintptr_t)b; return left<=right ? right-left<as : left-right<bs; }
bool xl6SlotMapInsertV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,const void* input,XL6_SlotMapKeyV1* output)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) return false;
    SlotMapOwner* owner=begin(value,layout); ptr item=NULL; XL6_SlotMapKeyV1 key={0}; bool success=false;
    if (!output || output->instance || output->slot ||
        (owner && input && overlaps(output,sizeof(*output),input,owner->layout.value.size))) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && (item=copyItem(owner,input,&scope))) {
        if (!xrtGetError() && !scope.failed && xl6SlotMapInstanceRetainV1(owner->instance)) {
            key.instance=owner->instance; key.slot=xrtSlotMapInsert(&owner->storage,item);
            if (key.slot) { item=NULL; success=true; }
            else xl6SlotMapInstanceDropV1(key.instance);
        }
    }
    if (item) { xrtTypeDropValue(storageType(owner),item); xrtFree(item); }
    success=finish(&scope,owner,success);
    if (success) *output=key;
    else if (key.slot) xl6SlotMapInstanceDropV1(key.instance);
    return success;
}
static bool outputEmpty(const SlotMapOwner* owner,const void* output)
{
    bool empty=output!=NULL;
    if (empty && owner->layout.value.transfer==2) empty=!((const xstrview*)output)->Data && !((const xstrview*)output)->Size;
    else if (empty && owner->layout.value.transfer==3) empty=!*(xvalue* const*)output;
    else if (empty && owner->layout.value.transfer==4) empty=!*(xfuture* const*)output;
    else if (empty && owner->layout.value.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1)
        empty=xl6SequenceNativeValuePolicyV1(&owner->policy)->Empty(output);
    if (!empty) xrtSetErrorKind(XERR_ARGUMENT);
    return empty;
}
bool xl6SlotMapReadV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,XL6_SlotMapKeyV1 key,void* output,bool* found)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) return false;
    SlotMapOwner* owner=begin(value,layout); bool success=false,result=false,copied=false;
    const xrttype* type=NULL; size_t size=0;
    if (!found || (owner && output && overlaps(output,owner->layout.value.size,found,sizeof(*found)))) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && outputEmpty(owner,output)) {
        type=storageType(owner); size=owner->layout.value.size; result=present(owner,key);
        success=!xrtGetError();
        if (success && result) {
            copied=true; /* An attempted copy can own a partial result on failure. */
            success=xrtTypeCopyValue(type,output,xrtSlotMapGet(&owner->storage,key.slot));
        }
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    else if (copied) {
        xerror* incoming=xrtErrorRef(xrtGetError()); xl6_error_scope cleanup={0};
        if (xl6ErrorScopeBeginCleanup(&cleanup,true)) {
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
bool xl6SlotMapReadRequiredV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,XL6_SlotMapKeyV1 key,void* output)
{
    bool found=false;
    if(!xl6SlotMapReadV1(value,layout,key,output,&found)) return false;
    if(!found) { xrtSetErrorKind(XERR_RANGE); return false; }
    return true;
}
bool xl6SlotMapSetV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,XL6_SlotMapKeyV1 key,const void* input,bool* updated)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) return false;
    SlotMapOwner* owner=begin(value,layout); bool success=false,result=false; ptr item=NULL;
    if (!updated) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && admits(owner,input)) {
        result=present(owner,key); success=!xrtGetError();
        if (success && result) {
            ptr old=xrtSlotMapGet(&owner->storage,key.slot); item=copyItem(owner,input,&scope);
            success=item && !xrtGetError() && !scope.failed && xrtSlotMapSet(&owner->storage,key.slot,item);
            if (success) { item=NULL; xrtTypeDropValue(storageType(owner),old); xrtFree(old); }
        }
    }
    if (item) { xrtTypeDropValue(storageType(owner),item); xrtFree(item); }
    success=finish(&scope,owner,success); if (success) *updated=result; return success;
}
bool xl6SlotMapKeyV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,XL6_SlotMapKeyV1 key,bool remove,bool* found)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) return false;
    SlotMapOwner* owner=begin(value,layout); bool success=false,result=false;
    if (!found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        result=present(owner,key); success=!xrtGetError();
        if (success && result && remove) {
            ptr item=NULL; success=xrtSlotMapRemove(&owner->storage,key.slot,&item);
            if (success) { xrtTypeDropValue(storageType(owner),item); xrtFree(item); }
        }
    }
    success=finish(&scope,owner,success); if (success) *found=result; return success;
}
bool xl6SlotMapClearV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) return false;
    SlotMapOwner* owner=begin(value,layout); bool success=false;
    if (owner) {
        xslotmapiter iterator={0}; success=xrtSlotMapIterBegin(&owner->storage,&iterator);
        if (success) {
            ptr item; while ((item=xrtSlotMapIterNext(&iterator,NULL))) {
                xrtTypeDropValue(storageType(owner),item); xrtFree(item);
                if (xrtGetError() || scope.failed) xl6ErrorScopeCaptureFailure(&scope);
            }
            xrtSlotMapIterEnd(&iterator); xrtSlotMapClear(&owner->storage);
        }
    }
    return finish(&scope,owner,success);
}
bool xl6SlotMapReserveV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,int64_t capacity)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) return false;
    SlotMapOwner* owner=begin(value,layout); bool success=false;
    if (capacity<0 || (uint64_t)capacity>UINT32_MAX) xrtSetErrorKind(XERR_RANGE);
    else if (owner) success=xrtSlotMapReserve(&owner->storage,(size_t)capacity);
    return finish(&scope,owner,success);
}
bool xl6SlotMapSizeV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,bool capacity,int64_t* output)
{
    xl6_error_scope scope={0}; if(!scopeBegin(&scope)) return false;
    SlotMapOwner* owner=begin(value,layout);
    if(!output) xrtSetErrorKind(XERR_ARGUMENT);
    size_t result=owner ? (capacity ? owner->storage.Storage.Capacity : owner->storage.Count) : 0;
    bool success=finish(&scope,owner,owner && output);
    if(success) *output=(int64_t)result;
    return success;
}
static int64_t measure(const xvalue* value,const XL6_SlotMapLayoutV1* layout,bool capacity)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) return -1;
    SlotMapOwner* owner=begin(value,layout); size_t result=owner ?
        (capacity ? owner->storage.Storage.Capacity : owner->storage.Count) : 0;
    return finish(&scope,owner,owner!=NULL) ? (int64_t)result : -1;
}
int64_t xl6SlotMapCountV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout) { return measure(value,layout,false); }
int64_t xl6SlotMapCapacityV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout) { return measure(value,layout,true); }

enum { SNAPSHOT_KEYS=1, SNAPSHOT_VALUES=2 };
/* One shared supplier traversal for unary and paired capture. The two lanes
 * are private typed owners, never parallel wire values or a second slot map.
 * No lane or partial row is published until both lanes and cleanup succeed. */
static bool snapshot(SlotMapOwner* owner,unsigned lanes,const XL6_SequenceLayoutV1 target[2],
    XL6_SequenceAdmissionV3 policy[2],const xvalueconverter* const converter[2],XL6_SlotMapSnapshotPairV1* result)
{
    xvalue* prepared[2]={0}; XL6_Sequence* output[2]={0}; xslotmapiter iterator={0}; bool success=true;
    for(unsigned lane=0;lane<2 && success;++lane) if(lanes & (1u<<lane)) {
        prepared[lane]=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_ARRAY_V1,target[lane].identity,&policy[lane]);
        if(prepared[lane]) output[lane]=xl6SequenceBeginRoleV1(prepared[lane],XL6_SEQUENCE_ARRAY_V1,target[lane].identity,
            target[lane].elementIdentity,target[lane].transfer,target[lane].size,target[lane].type);
        success=output[lane] && xl6SequenceBindProjectionV1(output[lane],converter[lane]) &&
            xrtTypedArrayReserve(xl6SequenceStorageV1(output[lane]),owner->storage.Count);
    }
    if (success && xrtSlotMapIterBegin(&owner->storage,&iterator)) {
        success=true; ptr item; xslot slot; size_t copied=0;
        while ((item=xrtSlotMapIterNext(&iterator,&slot))) {
            const XL6_SlotMapKeyV1 key={owner->instance,slot}; /* borrowed only under this pin */
            for(unsigned lane=0;lane<2 && success;++lane) if(lanes & (1u<<lane)) {
                const void* input=lane==0 ? (const void*)&key : item;
                success=xl6SequenceAdmitSlotV1(output[lane],input) &&
                    xrtTypedArrayPush(xl6SequenceStorageV1(output[lane]),input);
            }
            if(!success) break;
            ++copied;
        }
        success=success && copied==owner->storage.Count && !xrtGetError();
    } else success=false;
    xrtSlotMapIterEnd(&iterator);
    for(unsigned lane=0;lane<2;++lane) if(output[lane]) xl6SequenceEndV1(output[lane]);
    if (!success || xrtGetError()) { discard(prepared[0]); discard(prepared[1]); return false; }
    *result=(XL6_SlotMapSnapshotPairV1){prepared[0],prepared[1]}; return true;
}
static bool captureKeys(SlotMapOwner* owner,const xvalue* witness,
    XL6_SequenceLayoutV1* target,XL6_SequenceAdmissionV3* policy)
{
    XL6_SequenceRoleV1 role=0;
    if(!xl6SequenceSchemaCopyV1(witness,&role,target,policy)) return false;
    if (role!=XL6_SEQUENCE_ARRAY_V1 || target->elementIdentity!=XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID ||
        policy->boxedIdentity!=XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID || target->identity==owner->layout.identity ||
        target->transfer!=XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 ||
        target->size!=sizeof(XL6_SlotMapKeyV1) || target->type!=xl6SlotMapKeyTypeV1()) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    XL6_Sequence* schema=xl6SequenceBeginRoleV1(witness,role,target->identity,target->elementIdentity,
        target->transfer,target->size,target->type);
    bool empty=schema && xrtTypedArrayCount(xl6SequenceStorageV1(schema))==0;
    if(schema) xl6SequenceEndV1(schema);
    if(!empty && !xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT);
    return empty;
}
static bool captureValues(SlotMapOwner* owner,XL6_SequenceLayoutV1* target,XL6_SequenceAdmissionV3* policy)
{
    *target=owner->layout.value; *policy=owner->policy;
    if(policy->ops->Retain(policy->data)) return true;
    *policy=(XL6_SequenceAdmissionV3){0}; if(!xrtGetError()) xrtSetErrorKind(XERR_STATE);
    return false;
}
/* Result owners must retire while this operation still owns the first-error
 * accumulator, including a diagnostic-free failure in source-pin release.
 * Never discard a partial pair after publishing its checked return. */
static bool finishSnapshot(xl6_error_scope* scope,SlotMapOwner* owner,bool success,
    XL6_SlotMapSnapshotPairV1* prepared)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    if (owner) end(owner);
    if (scope->failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(scope);
        discard(prepared->keys); discard(prepared->values);
        *prepared=(XL6_SlotMapSnapshotPairV1){0};
    }
    return scopeFinish(scope,true);
}
xvalue* xl6SlotMapSnapshotTakeV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,
    bool keys,xvalue* witness,const xvalueconverter* converter)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) { discard(witness); return NULL; }
    SlotMapOwner* owner=begin(value,layout); XL6_SequenceAdmissionV3 policy[2]={0};
    XL6_SequenceLayoutV1 target[2]={0}; XL6_SlotMapSnapshotPairV1 prepared={0};
    const xvalueconverter* converters[2]={keys ? converter : NULL,keys ? NULL : converter};
    bool success=false;
    if (!keys && witness) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner && (keys ? captureKeys(owner,witness,&target[0],&policy[0]) :
        captureValues(owner,&target[1],&policy[1])))
        success=snapshot(owner,keys ? SNAPSHOT_KEYS : SNAPSHOT_VALUES,target,policy,converters,&prepared);
    xl6SequenceAdmissionDropV3(&policy[0]); xl6SequenceAdmissionDropV3(&policy[1]); discard(witness);
    if (!finishSnapshot(&scope,owner,success,&prepared)) return NULL;
    return keys ? prepared.keys : prepared.values;
}
xvalue* xl6SlotMapValuesV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,const xvalueconverter* converter)
{ return xl6SlotMapSnapshotTakeV1(value,layout,false,NULL,converter); }
bool xl6SlotMapSnapshotPairTakeV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout,
    xvalue* witness,const xvalueconverter* keyConverter,const xvalueconverter* valueConverter,XL6_SlotMapSnapshotPairV1* output)
{
    xl6_error_scope scope={0}; if(!scopeBegin(&scope)) { discard(witness); return false; }
    SlotMapOwner* owner=begin(value,layout); XL6_SequenceAdmissionV3 policy[2]={0};
    XL6_SequenceLayoutV1 target[2]={0}; XL6_SlotMapSnapshotPairV1 prepared={0}; bool success=false;
    const xvalueconverter* converters[2]={keyConverter,valueConverter};
    if(!output || output->keys || output->values) xrtSetErrorKind(XERR_ARGUMENT);
    else if(owner && captureKeys(owner,witness,&target[0],&policy[0]) &&
        captureValues(owner,&target[1],&policy[1]))
        success=snapshot(owner,SNAPSHOT_KEYS|SNAPSHOT_VALUES,target,policy,converters,&prepared);
    xl6SequenceAdmissionDropV3(&policy[0]); xl6SequenceAdmissionDropV3(&policy[1]); discard(witness);
    if(!finishSnapshot(&scope,owner,success,&prepared)) return false;
    *output=prepared; return true;
}
