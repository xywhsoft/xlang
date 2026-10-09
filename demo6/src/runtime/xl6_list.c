#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

typedef struct ListOwner {
    volatile int32_t refs;
    xatomic32 busy;
    XL6_ListLayoutV1 layout;
    XL6_SequenceAdmissionV3 policy;
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
/* This unpublished language carrier deliberately has no fabricated structural
 * Hash/Equal callback. Those checked contracts must be completed before the
 * source interner can use this owner or admit it as a typed Set/Dict key. */
static const xvaluehandleops handleOps={retain,drop,NULL,NULL};
static ListOwner* begin(const xvalue* value,const XL6_ListLayoutV1* expected,uint64_t cursor)
{
    ListOwner* owner=NULL;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
    uint64_t identity=cursor ? cursor : expected ? expected->identity : 0;
    if (value && expected && identity && xrtValueTypeId(value)==identity &&
        xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        ListOwner* candidate=raw; uint32 available=0;
        if (!candidate->cleared && candidate->storage && candidate->cursorIdentity==cursor &&
            sameLayout(&candidate->layout,expected) &&
            xrtAtomic32CompareExchange(&candidate->busy,&available,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
            if (xrtRefRetain(&candidate->refs)>0) owner=candidate;
            else xrtAtomic32Store(&candidate->busy,0,XMEMORY_RELEASE);
        }
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
    XL6_SequenceAdmissionV3* policy,uint64_t cursor,bool reverse,bool bounded,int64_t start)
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
        .cursorIdentity=cursor,.reverse=reverse,.bounded=bounded,.startKey=start};
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
xvalue* xl6ListCreateTakeV1(const XL6_ListLayoutV1* layout,xvalue* seed)
{
    xl6_error_scope scope={0};
    if (!(seed ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) {
        xrtValueRelease(seed); return NULL;
    }
    XL6_SequenceAdmissionV3 policy={0}; xvalue* result=NULL;
    if (xl6ListDomainCopyV1(layout,seed,&policy)) {
        xtypedlist* storage=xrtTypedListCreate(xl6SequenceStorageTypeV1(layout->value.transfer,layout->value.type));
        if (storage) result=wrap(storage,layout,&policy,0,false,false,0);
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(seed);
    if (!scopeFinish(&scope,result!=NULL)) { xrtValueRelease(result); return NULL; }
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
bool xl6ListKeyV1(const xvalue* value,const XL6_ListLayoutV1* layout,int64_t key,bool remove,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=begin(value,layout,0); bool success=false,result=false;
    if (!found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        result=xrtTypedListHas(owner->storage,key);
        success=!xrtGetError() && (!result || !remove || xrtTypedListRemove(owner->storage,key));
    }
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
static xvalue* clone(const xvalue* value,const XL6_ListLayoutV1* layout,uint64_t cursor,
    bool reverse,bool bounded,int64_t start)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    ListOwner* owner=begin(value,layout,0); xvalue* result=NULL; XL6_SequenceAdmissionV3 policy={0};
    if (owner && owner->policy.ops->Retain(owner->policy.data)) {
        policy=owner->policy; xtypedlist* storage=xrtTypedListClone(owner->storage);
        if (storage) result=wrap(storage,layout,&policy,cursor,reverse,bounded,start);
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&policy);
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
bool xl6ListCursorAdvanceV1(const xvalue* value,const XL6_ListLayoutV1* layout,uint64_t identity,bool* found)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return false;
    ListOwner* owner=identity ? begin(value,layout,identity) : NULL;
    bool success=false,result=false;
    if (!identity || !found) xrtSetErrorKind(XERR_ARGUMENT);
    else if (owner) {
        if (owner->failed) xrtSetErrorKind(XERR_STATE);
        else if (owner->ended) success=true;
        else {
            if (!owner->started) {
                owner->started=owner->bounded ?
                    (owner->reverse ? xrtTypedListIterRFrom(owner->storage,owner->startKey,&owner->iterator) :
                        xrtTypedListIterFrom(owner->storage,owner->startKey,&owner->iterator)) :
                    (owner->reverse ? xrtTypedListIterRBegin(owner->storage,&owner->iterator) :
                        xrtTypedListIterBegin(owner->storage,&owner->iterator));
                if (!owner->started) { owner->failed=true; goto done; }
            }
            owner->current=xrtTypedListIterNext(&owner->iterator,&owner->currentKey);
            success=!xrtGetError(); result=success && owner->current;
            if (!success) owner->failed=true;
            if (!owner->current || !success) cursorEnd(owner);
        }
    }
done:
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
