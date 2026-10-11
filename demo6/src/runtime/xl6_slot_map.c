#include "xl6_slot_map.h"
#include "xl6_slot_map_key.h"
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

static bool scopeFinish(xl6_error_scope* scope,bool success)
{
    if (!success || xrtGetError() || scope->failed) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result=xl6ErrorScopeEnd(scope);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    return result.ok;
}
static bool scopeBegin(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
}
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
        xrtErrorFree(incoming); xrtValueRelease(value); (void)scopeFinish(&cleanup,true);
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
    if (entered) (void)scopeFinish(&cleanup,true);
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
    if (!scopeBegin(&work)) { (void)scopeFinish(&cleanup,false); return false; }
    xslotmap retired=owner->retired; owner->retired=(xslotmap){0}; storageDestroy(&retired,storageType(owner),&work);
    bool success=scopeFinish(&work,true); (void)scopeFinish(&cleanup,true); return success;
}
const xrtownershipadapterv1* xl6SlotMapOwnershipAdapterV2(xrtownershipref ref,
    const XL6_SequenceAdmissionOpsV2* const* allowed,size_t count)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),ownershipHold,ownershipDrop,ownershipClaim,
        ownershipRestore,NULL,ownershipClear,ownershipFinish};
    if (ref.Data && ref.Ops==&ownershipOps) {
        size_t refs; if (!ownershipCount(ref.Data,&refs)) return NULL;
        const SlotMapOwner* owner=ref.Data;
        if (owner->layout.value.transfer==4) {
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
    if (!scopeFinish(&scope,result!=NULL)) { discard(result); return NULL; }
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
static int64_t measure(const xvalue* value,const XL6_SlotMapLayoutV1* layout,bool capacity)
{
    xl6_error_scope scope={0}; if (!scopeBegin(&scope)) return -1;
    SlotMapOwner* owner=begin(value,layout); size_t result=owner ?
        (capacity ? owner->storage.Storage.Capacity : owner->storage.Count) : 0;
    return finish(&scope,owner,owner!=NULL) ? (int64_t)result : -1;
}
int64_t xl6SlotMapCountV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout) { return measure(value,layout,false); }
int64_t xl6SlotMapCapacityV1(const xvalue* value,const XL6_SlotMapLayoutV1* layout) { return measure(value,layout,true); }
