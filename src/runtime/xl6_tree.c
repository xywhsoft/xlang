#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_tree_projection.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct TreeOwner {
    volatile int32_t refs;
    xatomic32 busy;
    XL6_TreeLayoutV2 layout;
    XL6_SequenceAdmissionV3 keyPolicy, valuePolicy;
    xtypedtree* storage;
    xtypedtree* retired;
    const void* claim;
    bool cleared;
} TreeOwner;
typedef struct TreeTrace {
    const TreeOwner* owner;
    xrtownershipvisitor visit;
    void* context;
    bool failed;
} TreeTrace;

static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }
static void scopeBegin(xl6_error_scope* scope)
{ if (!xl6ErrorScopeBegin(scope)) abort(); }
static xl6_error_result scopeOutcome(xl6_error_scope* scope, bool success)
{
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    return xl6ErrorScopeEnd(scope);
}
/* Payload-only Tree operations are bracketed by the IR FOREIGN_SCOPE ABI;
 * void finalizers also need an independent notification, even with no error
 * allocation. Keep that contract distinct from STATUS_OUTPUT queries. */
static bool scopeFinish(xl6_error_scope* scope, bool success)
{
    xl6_error_result result=scopeOutcome(scope,success);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
    return result.ok;
}
static bool scopeFinishChecked(xl6_error_scope* scope, bool success)
{
    xl6_error_result result=scopeOutcome(scope,success);
    if (!result.ok) xrtSetErrorTake(result.error);
    return result.ok;
}
static bool sameLayout(const XL6_SequenceLayoutV1* a,const XL6_SequenceLayoutV1* b)
{
    return a->identity==b->identity && a->elementIdentity==b->elementIdentity &&
        a->transfer==b->transfer && a->size==b->size && a->type==b->type;
}
static bool sameTreeLayout(const XL6_TreeLayoutV2* a,const XL6_TreeLayoutV2* b)
{
    return a && b && a->identity==b->identity && a->keyOrder==b->keyOrder &&
        a->keyCodec==b->keyCodec &&
        sameLayout(&a->key,&b->key) && sameLayout(&a->value,&b->value);
}
static const xrttype* orderedKeyType(const XL6_TreeLayoutV2* layout)
{ return layout->keyCodec ? layout->keyCodec->type : layout->key.type; }
static bool bytesCodecValid(const XL6_TreeKeyCodecV1* codec)
{
    const xrttype* type=codec ? codec->type : NULL;
    return codec && codec->abi==XL6_TREE_BYTES_CODEC_ABI_V1 && codec->Admits && type &&
        type->Id==XL6_TREE_BYTES_CODEC_ABI_V1 && type->Kind==XRT_TYPE_HANDLE &&
        type->Size==sizeof(xvalue*) && type->Align==_Alignof(xvalue*) && type->InstanceAlign==1 &&
        type->Flags==(XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_REFERENCE|XRT_TYPE_FLAG_NULLABLE|
            XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE) &&
        type->AbiName.Size==sizeof("xl6.tree-bytes-key.v1")-1 && type->AbiName.Data &&
        !memcmp(type->AbiName.Data,"xl6.tree-bytes-key.v1",sizeof("xl6.tree-bytes-key.v1")-1) &&
        type->Ops && type->Ops->Init && type->Ops->Copy && type->Ops->Move && type->Ops->Drop && type->Ops->Compare;
}
static bool cScalarType(const xrttype* type)
{
    return type==xrtTypeBool() || type==xrtTypeInt8() || type==xrtTypeUInt8() ||
        type==xrtTypeInt16() || type==xrtTypeUInt16() || type==xrtTypeInt32() || type==xrtTypeUInt32() ||
        type==xrtTypeInt64() || type==xrtTypeUInt64() || type==xrtTypeFloat32() || type==xrtTypeFloat64();
}
static bool samePolicy(const XL6_SequenceAdmissionV3* a,const XL6_SequenceAdmissionV3* b)
{
    return a->ops && a->ops==b->ops && a->elementIdentity==b->elementIdentity &&
        a->transfer==b->transfer && a->size==b->size && a->type==b->type &&
        a->boxedIdentity==b->boxedIdentity && a->scalarDomain==b->scalarDomain &&
        (a->data==b->data || a->ops->Equivalent(a->data,b->data));
}
static bool layoutValid(const XL6_TreeLayoutV2* layout)
{
    if (!layout || !layout->identity || !layout->keyOrder ||
        layout->key.transfer>3 || layout->value.transfer>3 ||
        !xl6SequenceTupleV1(layout->key.identity,layout->key.elementIdentity,
            layout->key.transfer,layout->key.size,layout->key.type) ||
        !xl6SequenceTupleV1(layout->value.identity,layout->value.elementIdentity,
            layout->value.transfer,layout->value.size,layout->value.type) ||
        (layout->keyOrder==XL6_TREE_KEY_BYTES_V1 ?
            layout->key.transfer!=3 || layout->key.type!=xrtTypeValue() || !bytesCodecValid(layout->keyCodec) :
            layout->keyCodec!=NULL || layout->key.transfer>2 || !xrtTypeIsComparable(layout->key.type))) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    const xrttype* type=layout->key.type;
    bool ordered=false;
    switch (layout->keyOrder) {
    case XL6_TREE_KEY_BOOL_V1: ordered=type==xrtTypeBool(); break;
    case XL6_TREE_KEY_SIGNED_I64_V1: ordered=type==xrtTypeInt64() || type==xrtTypeTime(); break;
    case XL6_TREE_KEY_UNSIGNED_U64_V1: ordered=type==xrtTypeUInt64(); break;
    case XL6_TREE_KEY_FLOAT_TOTAL_V1: ordered=type==xrtTypeFloat32() || type==xrtTypeFloat64(); break;
    case XL6_TREE_KEY_STRING_V1: ordered=xl6StringTypeAbiV1(type); break;
    case XL6_TREE_KEY_TYPE_ID_V1: ordered=type==xrtTypeType(); break;
    case XL6_TREE_KEY_CHAR_V1: ordered=xl6CharTypeAbiV1(type); break;
    case XL6_TREE_KEY_C_SCALAR_V1: ordered=layout->key.transfer==1 && cScalarType(type); break;
    case XL6_TREE_KEY_BYTES_V1: ordered=true; break;
    }
    if (!ordered) xrtSetErrorKind(XERR_TYPE);
    return ordered;
}

static void drop(void* opaque,void* context)
{
    (void)context;
    TreeOwner* owner=opaque;
    if (!owner) return;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    int32_t refs=xrtRefRelease(&owner->refs);
    if (refs<0) abort();
    if (refs) { mutationEnd(&mutation); return; }
    if (owner->retired || xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    xtypedtree* storage=owner->storage; owner->storage=NULL;
    mutationEnd(&mutation);
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBeginCleanup(&scope,false)) abort();
    xrtTypedTreeDestroy(storage);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&owner->valuePolicy);
    xl6SequenceAdmissionDropV3(&owner->keyPolicy);
    xrtFree(owner);
    (void)scopeFinish(&scope,true);
}
static bool retain(void* opaque,void** result,void* context)
{
    (void)context;
    TreeOwner* owner=opaque;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    bool success=owner && result && !owner->cleared && xrtRefRetain(&owner->refs)>0;
    if (success) *result=owner;
    else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation);
    return success;
}
/* Explicit Tree.equals has structural value equality. Dynamic handle identity
 * remains the existing reference contract; no new key Hash/order is invented. */
static const xvaluehandleops handleOps={retain,drop,NULL,NULL};

static TreeOwner* begin(const xvalue* value,const XL6_TreeLayoutV2* expected)
{
    TreeOwner* owner=NULL;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    void* raw=NULL; void* context=NULL; const xvaluehandleops* actual=NULL;
    /* Check the resident operation table before touching an opaque pointer. */
    if (value && expected && xrtValueTypeId(value)==expected->identity &&
        xrtValueGetHandle(value,&raw,&actual,&context) && actual==&handleOps && raw && !context) {
        TreeOwner* candidate=raw;
        uint32 available=0;
        /* A retirement claim reserves the owner, not an already-cleared
         * container. Language finalizers must still observe intact fields.
         * BUSY/cleared provide the same access boundary as resident Sequence. */
        if (!candidate->cleared && candidate->storage &&
            sameTreeLayout(&candidate->layout,expected) &&
            xrtAtomic32CompareExchange(&candidate->busy,&available,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
            if (xrtRefRetain(&candidate->refs)>0) owner=candidate;
            else xrtAtomic32Store(&candidate->busy,0,XMEMORY_RELEASE);
        }
    }
    if (!owner) xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation);
    return owner;
}
static void end(TreeOwner* owner)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE);
    mutationEnd(&mutation); drop(owner,NULL);
}
static bool finish(xl6_error_scope* scope,TreeOwner* owner,bool success)
{
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    if (owner) end(owner);
    return scopeFinish(scope,true);
}
static bool slotAdmits(const XL6_SequenceAdmissionV3* policy,const void* slot)
{
    bool success=slot && policy->ops->Slot(policy->data,slot);
    if (!success && !xrtGetError()) xrtSetErrorKind(XERR_TYPE);
    return success;
}

/* The shell owns one counted wrapper; the wrapper visits every actual Value
 * slot once, including duplicates. Never flatten two shells into double edges. */
static bool ownershipCount(const void* opaque,size_t* result)
{
    const TreeOwner* owner=opaque;
    if (!owner || !result || owner->refs<=0 || owner->cleared || !owner->storage ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) return false;
    *result=(size_t)owner->refs; return true;
}
static bool traceEntry(ptr entry,ptr opaque)
{
    TreeTrace* trace=opaque;
    const TreeOwner* owner=trace->owner;
    if (owner->layout.key.transfer==3) {
        const xvalue* key=*(xvalue* const*)entry;
        if (!trace->visit(xrtValueOwnership(key),trace->context)) { trace->failed=true; return false; }
    }
    if (owner->layout.value.transfer==3) {
        const xvalue* value=*(xvalue* const*)((const unsigned char*)entry+owner->storage->ValueOffset);
        if (!trace->visit(xrtValueOwnership(value),trace->context)) { trace->failed=true; return false; }
    }
    return true;
}
static bool ownershipTrace(const void* opaque,xrtownershipvisitor visit,void* context)
{
    const TreeOwner* owner=opaque; size_t refs;
    if (!visit || !ownershipCount(opaque,&refs)) return false;
    if (owner->layout.key.transfer!=3 && owner->layout.value.transfer!=3) return true;
    TreeTrace trace={owner,visit,context,false};
    size_t count=owner->storage->Storage.Base.Count;
    size_t visited=xrtAVLTreeVisit(&owner->storage->Storage,traceEntry,&trace);
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
    TreeOwner* owner=(TreeOwner*)opaque;
    return owner->refs>0 && owner->refs<INT32_MAX && xrtRefRetain(&owner->refs)>0;
}
static void ownershipDrop(const void* opaque) { drop((void*)opaque,NULL); }
static bool ownershipClaim(const void* opaque,const void* token)
{
    TreeOwner* owner=(TreeOwner*)opaque;
    if (!token || owner->cleared || (owner->claim && owner->claim!=token)) return false;
    owner->claim=token; return true;
}
static void ownershipRestore(const void* opaque,const void* token)
{
    TreeOwner* owner=(TreeOwner*)opaque;
    if (!token || owner->claim!=token || owner->cleared) abort();
    owner->claim=NULL;
}
static void ownershipClear(const void* opaque,const void* token)
{
    TreeOwner* owner=(TreeOwner*)opaque;
    if (!token || owner->claim!=token || owner->cleared || owner->retired ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    owner->retired=owner->storage; owner->storage=NULL; owner->cleared=true;
}
static bool ownershipFinish(const void* opaque,const void* token)
{
    TreeOwner* owner=(TreeOwner*)opaque;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    if (!token || owner->claim!=token || !owner->cleared) abort();
    xtypedtree* retired=owner->retired; owner->retired=NULL;
    mutationEnd(&mutation);
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBeginCleanup(&scope,false)) abort();
    /* A previously failed caller scope is not this duty's completion result.
     * Returning false after a successful destroy would request an endless
     * retry of storage that has already been physically retired. */
    xl6_error_scope work={0}; scopeBegin(&work);
    xrtTypedTreeDestroy(retired);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&work);
    xl6_error_result completed=xl6ErrorScopeEnd(&work);
    bool success=completed.ok;
    if (!success) xl6ErrorScopePropagateFailure(completed);
    (void)scopeFinish(&scope,true);
    return success;
}
const xrtownershipadapterv1* xl6TreeOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),ownershipHold,ownershipDrop,
        ownershipClaim,ownershipRestore,NULL,ownershipClear,ownershipFinish};
    size_t refs;
    if (ref.Data && ref.Ops==&ownershipOps) return ownershipCount(ref.Data,&refs) ? &adapter : NULL;
    return xrtValueHandleOwnershipAdapterV1(ref,&handleOps,handleTrace);
}

/* All three inputs are consumed. Called only with metadata copied from genuine
 * admitted owners. Publication binds the complete resident graph capability. */
static xvalue* wrap(xtypedtree* storage,const XL6_TreeLayoutV2* layout,
    XL6_SequenceAdmissionV3* key,XL6_SequenceAdmissionV3* value)
{
    xl6_error_scope scope={0}; scopeBegin(&scope);
    TreeOwner* owner=xrtMalloc(sizeof(*owner));
    if (!owner) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&scope);
        xrtTypedTreeDestroy(storage);
        xl6SequenceAdmissionDropV3(value); xl6SequenceAdmissionDropV3(key);
        (void)scopeFinish(&scope,true);
        return NULL;
    }
    *owner=(TreeOwner){.refs=1,.layout=*layout,.keyPolicy=*key,.valuePolicy=*value,.storage=storage};
    *key=(XL6_SequenceAdmissionV3){0}; *value=(XL6_SequenceAdmissionV3){0};
    xrtAtomic32Init(&owner->busy,0);
    xvalue* result=xrtValueHandleTake((void**)&owner,&handleOps,NULL);
    if (!result) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        xl6ErrorScopeCaptureFailure(&scope); drop(owner,NULL);
        (void)scopeFinish(&scope,true); return NULL;
    }
    if (!xrtValueHandleOwnershipBindPhased(result,handleTrace) || !xrtValueTypeIdBind(result,layout->identity)) {
        xl6ErrorScopeCaptureFailure(&scope); xrtValueRelease(result);
        (void)scopeFinish(&scope,true); return NULL;
    }
    if (!scopeFinish(&scope,true)) { xrtValueRelease(result); return NULL; }
    return result;
}
static bool witnessCopy(const xvalue* witness,const XL6_SequenceLayoutV1* expected,
    XL6_SequenceAdmissionV3* policy)
{
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 actual={0};
    if (!xl6SequenceSchemaCopyV1(witness,&role,&actual,policy)) return false;
    if (role!=XL6_SEQUENCE_ARRAY_V1 || !sameLayout(&actual,expected)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(witness,role,actual.identity,actual.elementIdentity,
        actual.transfer,actual.size,actual.type);
    if (!pin) return false;
    bool empty=xrtTypedArrayCount(xl6SequenceStorageV1(pin))==0;
    xl6SequenceEndV1(pin);
    if (!empty) xrtSetErrorKind(XERR_ARGUMENT);
    return empty;
}
static bool emptyPolicy(const XL6_SequenceAdmissionV3* policy)
{
    return policy && !policy->ops && !policy->data && !policy->elementIdentity &&
        !policy->transfer && !policy->size && !policy->type && !policy->boxedIdentity && !policy->scalarDomain;
}
bool xl6TreeDomainsCopyV1(const XL6_TreeLayoutV2* layout,const xvalue* keys,const xvalue* values,
    XL6_SequenceAdmissionV3* keyOut,XL6_SequenceAdmissionV3* valueOut)
{
    if (!keys || !values || keyOut==valueOut || !emptyPolicy(keyOut) || !emptyPolicy(valueOut)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    XL6_SequenceAdmissionV3 key={0},value={0};
    bool success=layoutValid(layout) && witnessCopy(keys,&layout->key,&key) &&
        witnessCopy(values,&layout->value,&value);
    if (success && layout->keyCodec && !layout->keyCodec->Admits(&key,layout->key.elementIdentity)) {
        xrtSetErrorKind(XERR_TYPE);success=false;
    }
    if (success) { *keyOut=key; *valueOut=value; }
    else { xl6SequenceAdmissionDropV3(&value); xl6SequenceAdmissionDropV3(&key); }
    return success;
}
bool xl6TreeAdmitsSchemaV1(const xvalue* value,const XL6_TreeLayoutV2* layout,
    const XL6_SequenceAdmissionV3* key,const XL6_SequenceAdmissionV3* item)
{
    if (!value || !layout || !key || !item || !key->ops || !item->ops ||
        xrtValueType(value)!=XVALUE_HANDLE || xrtValueTypeId(value)!=layout->identity) return false;
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
    bool admitted=false;
    if (xrtValueGetHandle(value,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
        const TreeOwner* owner=raw;
        admitted=!owner->cleared && sameTreeLayout(&owner->layout,layout) &&
            samePolicy(&owner->keyPolicy,key) && samePolicy(&owner->valuePolicy,item);
    }
    mutationEnd(&mutation); return admitted;
}
bool xl6TreeSchemaCopyV1(const xvalue* source,XL6_TreeLayoutV2* layout,
    XL6_SequenceAdmissionV3* keyOut,XL6_SequenceAdmissionV3* valueOut)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    XL6_SequenceAdmissionV3 key={0},value={0}; XL6_TreeLayoutV2 captured={0}; bool ok=false;
    if(!source || !layout || keyOut==valueOut || !emptyPolicy(keyOut) || !emptyPolicy(valueOut))
        xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xrtownershipscope mutation={0}; mutationBegin(&mutation);
        void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
        if(xrtValueGetHandle(source,&raw,&actual,&user) && actual==&handleOps && raw && !user) {
            const TreeOwner* owner=raw;
            if(owner->cleared || !owner->storage || xrtValueTypeId(source)!=owner->layout.identity)
                xrtSetErrorKind(XERR_STATE);
            else if(owner->keyPolicy.ops->Retain(owner->keyPolicy.data)) {
                key=owner->keyPolicy;
                if(owner->valuePolicy.ops->Retain(owner->valuePolicy.data)) {
                    value=owner->valuePolicy; captured=owner->layout; ok=true;
                }
            }
        } else xrtSetErrorKind(XERR_ARGUMENT);
        mutationEnd(&mutation);
    }
    if(!ok || xrtGetError() || scope.failed) {
        xl6ErrorScopeCaptureFailure(&scope);
        xl6SequenceAdmissionDropV3(&value); xl6SequenceAdmissionDropV3(&key);
        (void)scopeFinish(&scope,false); return false;
    }
    if(!scopeFinish(&scope,true)) {
        xerror* first=xrtTakeError(); xl6SequenceAdmissionDropV3(&value); xl6SequenceAdmissionDropV3(&key);
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,first}); return false;
    }
    *layout=captured; *keyOut=key; *valueOut=value; return true;
}
xvalue* xl6TreeCreateTakeV1(const XL6_TreeLayoutV2* layout,xvalue* keys,xvalue* values)
{
    xl6_error_scope scope={0};
    if (!(keys && values ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) abort();
    XL6_SequenceAdmissionV3 key={0},value={0}; xvalue* result=NULL;
    if (xl6TreeDomainsCopyV1(layout,keys,values,&key,&value)) {
        xtypedtree* storage=xrtTypedTreeCreate(orderedKeyType(layout),
            xl6SequenceStorageTypeV1(layout->value.transfer,layout->value.type));
        if (storage) result=wrap(storage,layout,&key,&value);
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&value); xl6SequenceAdmissionDropV3(&key);
    xrtValueRelease(values); xrtValueRelease(keys);
    if (!scopeFinish(&scope,result!=NULL)) { xrtValueRelease(result); return NULL; }
    return result;
}
bool xl6TreeSetV1(const xvalue* value,const XL6_TreeLayoutV2* layout,const void* key,const void* item)
{
    xl6_error_scope scope={0}; scopeBegin(&scope); TreeOwner* owner=begin(value,layout);
    bool success=owner && slotAdmits(&owner->keyPolicy,key) && slotAdmits(&owner->valuePolicy,item) &&
        xrtTypedTreeSet(owner->storage,key,item);
    return finish(&scope,owner,success);
}
bool xl6TreeGetV1(const xvalue* value,const XL6_TreeLayoutV2* layout,const void* key,
    void* output,bool take,bool* found)
{
    xl6_error_scope scope={0}; scopeBegin(&scope); TreeOwner* owner=begin(value,layout);
    bool success=false,result=false;
    if (owner && output && found && slotAdmits(&owner->keyPolicy,key)) {
        const void* source=xrtTypedTreeConstGet(owner->storage,key);
        if (!source) success=!xrtGetError();
        else {
            success=take ? xrtTypedTreeTake(owner->storage,key,output) :
                xrtTypeCopyValue(xl6SequenceStorageTypeV1(layout->value.transfer,layout->value.type),output,source);
            result=success;
        }
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
bool xl6TreeKeyV1(const xvalue* value,const XL6_TreeLayoutV2* layout,const void* key,bool remove,bool* found)
{
    xl6_error_scope scope={0}; scopeBegin(&scope); TreeOwner* owner=begin(value,layout);
    bool success=false,result=false;
    if (owner && found && slotAdmits(&owner->keyPolicy,key)) {
        result=xrtTypedTreeHas(owner->storage,key);
        success=!xrtGetError() && (!result || !remove || xrtTypedTreeRemove(owner->storage,key));
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
static bool overlaps(const void* left,size_t leftSize,const void* right,size_t rightSize)
{
    uintptr_t a=(uintptr_t)left,b=(uintptr_t)right;
    if (a>UINTPTR_MAX-leftSize || b>UINTPTR_MAX-rightSize) return true;
    return a<b+rightSize && b<a+leftSize;
}
static bool querySlotShape(XL6_TreeQuerySlotV2 slot,const xrttype* type)
{
    return slot.data && type && slot.size==type->Size && type->Align &&
        (uintptr_t)slot.data%type->Align==0 && (uintptr_t)slot.data<=UINTPTR_MAX-slot.size;
}
static bool querySlotEmpty(const XL6_SequenceLayoutV1* layout,const void* slot)
{
    /* Only managed slots require an empty-owner check. Do not compare struct
     * padding or invent a bit-pattern default for trivial nullable records. */
    if (layout->transfer==3) return *(xvalue* const*)slot==NULL;
    if (layout->transfer==2) {
        const xstrview* text=slot;
        return text->Data==NULL && text->Size==0;
    }
    return true;
}
static bool querySlotsValid(const XL6_TreeLayoutV2* layout,XL6_TreeQueryV2 operation,
    const void* search,XL6_TreeQuerySlotV2 key,XL6_TreeQuerySlotV2 value,bool* found)
{
    bool pair=operation>=XL6_TREE_QUERY_FIRST_V2;
    bool searches=operation<=XL6_TREE_QUERY_TAKE_V2 || operation>=XL6_TREE_QUERY_LOWER_V2;
    const xrttype* item=xl6SequenceStorageTypeV1(layout->value.transfer,layout->value.type);
    if (operation>XL6_TREE_QUERY_UPPER_V2 || (int)operation<0 || !found ||
        (searches ? !search : search!=NULL) || !querySlotShape(value,item) ||
        (pair ? !querySlotShape(key,layout->key.type) : key.data!=NULL || key.size!=0) ||
        (uintptr_t)found%_Alignof(bool) || (uintptr_t)found>UINTPTR_MAX-sizeof(*found) ||
        (searches && ((uintptr_t)search%layout->key.type->Align ||
            (uintptr_t)search>UINTPTR_MAX-layout->key.size))) return false;
    if (overlaps(value.data,value.size,found,sizeof(*found)) ||
        overlaps(value.data,value.size,layout,sizeof(*layout)) ||
        overlaps(found,sizeof(*found),layout,sizeof(*layout)) ||
        (searches && (overlaps(value.data,value.size,search,layout->key.size) ||
            overlaps(found,sizeof(*found),search,layout->key.size))) ||
        (pair && (overlaps(key.data,key.size,value.data,value.size) ||
            overlaps(key.data,key.size,found,sizeof(*found)) ||
            overlaps(key.data,key.size,layout,sizeof(*layout)) ||
            (searches && overlaps(key.data,key.size,search,layout->key.size))))) return false;
    return querySlotEmpty(&layout->value,value.data) && (!pair || querySlotEmpty(&layout->key,key.data));
}
bool xl6TreeQueryV2(const xvalue* object,const XL6_TreeLayoutV2* layout,XL6_TreeQueryV2 operation,
    const void* search,bool required,XL6_TreeQuerySlotV2 keyOutput,
    XL6_TreeQuerySlotV2 valueOutput,bool* found)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) return false;
    TreeOwner* owner=begin(object,layout);
    bool pair=operation>=XL6_TREE_QUERY_FIRST_V2;
    bool keyReady=false,valueReady=false,success=false,present=false;
    const xrttype* keyType=owner ? orderedKeyType(&owner->layout) : NULL;
    const xrttype* valueType=owner ? xl6SequenceStorageTypeV1(owner->layout.value.transfer,owner->layout.value.type) : NULL;
    if (!owner) goto done;
    if (!querySlotsValid(&owner->layout,operation,search,keyOutput,valueOutput,found) ||
        /* The borrowed caller layout is also an input; it must not be an output. */
        overlaps(valueOutput.data,valueOutput.size,layout,sizeof(*layout)) ||
        overlaps(found,sizeof(*found),layout,sizeof(*layout)) ||
        (pair && overlaps(keyOutput.data,keyOutput.size,layout,sizeof(*layout)))) {
        xrtSetErrorKind(XERR_ARGUMENT); goto done;
    }
    if (search && !slotAdmits(&owner->keyPolicy,search)) goto done;
    *found=false;
    if (pair) {
        memset(keyOutput.data,0,keyOutput.size);
        keyReady=true;
    }
    /* These closed resident slot codecs accept an empty acquisition target.
     * Init would construct a real default T; string Init allocates and would
     * incorrectly turn a missing result into an owned empty string. */
    memset(valueOutput.data,0,valueOutput.size);
    valueReady=true;
    const void* key=NULL; const void* value=NULL;
    switch (operation) {
    case XL6_TREE_QUERY_GET_V2:
    case XL6_TREE_QUERY_TAKE_V2: value=xrtTypedTreeConstGet(owner->storage,search); break;
    case XL6_TREE_QUERY_FIRST_V2: value=xrtTypedTreeFirst(owner->storage,&key); break;
    case XL6_TREE_QUERY_LAST_V2: value=xrtTypedTreeLast(owner->storage,&key); break;
    case XL6_TREE_QUERY_LOWER_V2: value=xrtTypedTreeLowerBound(owner->storage,search,&key); break;
    case XL6_TREE_QUERY_UPPER_V2: value=xrtTypedTreeUpperBound(owner->storage,search,&key); break;
    }
    if (!value) {
        if (!xrtGetError() && required) xrtSetErrorKind(XERR_RANGE);
        success=!required && !xrtGetError(); goto done;
    }
    /* Each private result is its own staging area. A failed second copy drops
     * the first; unlike caller-variable writeback no heap scratch is needed. */
    if (pair && !xrtTypeCopyValue(keyType,keyOutput.data,key)) goto done;
    success=operation==XL6_TREE_QUERY_TAKE_V2 ? xrtTypedTreeTake(owner->storage,search,valueOutput.data) :
        xrtTypeCopyValue(valueType,valueOutput.data,value);
    present=success;
done:
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (owner) end(owner);
    if (scope.failed || xrtGetError()) {
        xl6ErrorScopeCaptureFailure(&scope);
        /* Drops may report a second failure. The first outcome is already
         * latched; detach every initialized result before returning failure. */
        if (valueReady) { xrtTypeDropValue(valueType,valueOutput.data); memset(valueOutput.data,0,valueOutput.size); }
        if (keyReady) { xrtTypeDropValue(keyType,keyOutput.data); memset(keyOutput.data,0,keyOutput.size); }
    }
    success=scopeFinishChecked(&scope,true);
    if (success) *found=present;
    return success;
}
static bool copyPair(TreeOwner* owner,const void* key,const void* value,void* keyOut,void* valueOut)
{
    xl6_error_scope scope={0}; scopeBegin(&scope);
    const xrttype* keyType=orderedKeyType(&owner->layout);
    const xrttype* valueType=xl6SequenceStorageTypeV1(owner->layout.value.transfer,owner->layout.value.type);
    void* keyTemp=NULL;
    void* valueTemp=NULL;
    bool keyReady=false,valueReady=false,success=false;
    keyTemp=xrtMalloc(owner->layout.key.size);
    if (!keyTemp) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    valueTemp=xrtMalloc(owner->layout.value.size);
    if (!valueTemp) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    if (!xrtTypeInitValue(keyType,keyTemp)) goto done;
    keyReady=true;
    if (!xrtTypeInitValue(valueType,valueTemp)) goto done;
    valueReady=true;
    if (!xrtTypeCopyValue(keyType,keyTemp,key) || !xrtTypeCopyValue(valueType,valueTemp,value)) goto done;
    /* Resident relocatable codecs cannot fail an owning move. Keep both
     * copies until this publication point; no partial result on copy/OOM. */
    if (!xrtTypeMoveValue(keyType,keyOut,keyTemp) || !xrtTypeMoveValue(valueType,valueOut,valueTemp)) abort();
    success=true;
done:
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (valueReady) xrtTypeDropValue(valueType,valueTemp);
    if (keyReady) xrtTypeDropValue(keyType,keyTemp);
    xrtFree(valueTemp); xrtFree(keyTemp);
    return scopeFinish(&scope,true);
}
bool xl6TreeEndpointV1(const xvalue* object,const XL6_TreeLayoutV2* layout,unsigned int operation,
    const void* search,void* keyOutput,void* valueOutput,bool* found)
{
    xl6_error_scope scope={0}; scopeBegin(&scope); TreeOwner* owner=begin(object,layout);
    bool success=false,result=false;
    if (owner && found && keyOutput && valueOutput && operation<=3 &&
        (operation<2 ? !search : slotAdmits(&owner->keyPolicy,search))) {
        if (overlaps(keyOutput,layout->key.size,valueOutput,layout->value.size)) success=true;
        else {
            const void* key=NULL; const void* value=NULL;
            switch (operation) {
            case 0: value=xrtTypedTreeFirst(owner->storage,&key); break;
            case 1: value=xrtTypedTreeLast(owner->storage,&key); break;
            case 2: value=xrtTypedTreeLowerBound(owner->storage,search,&key); break;
            case 3: value=xrtTypedTreeUpperBound(owner->storage,search,&key); break;
            }
            success=value ? copyPair(owner,key,value,keyOutput,valueOutput) : !xrtGetError();
            result=success && value;
        }
    }
    success=finish(&scope,owner,success);
    if (success) *found=result;
    return success;
}
static bool compatiblePolicies(const TreeOwner* a,const TreeOwner* b)
{
    if (sameTreeLayout(&a->layout,&b->layout) && samePolicy(&a->keyPolicy,&b->keyPolicy) &&
        samePolicy(&a->valuePolicy,&b->valuePolicy)) return true;
    xrtSetErrorKind(XERR_TYPE); return false;
}
static TreeOwner* beginOther(const xvalue* other,const XL6_TreeLayoutV2* layout,TreeOwner* first)
{
    if (first) {
        void* raw=NULL; void* user=NULL; const xvaluehandleops* actual=NULL;
        if (other && xrtValueTypeId(other)==layout->identity && xrtValueGetHandle(other,&raw,&actual,&user) &&
            raw==first && actual==&handleOps && !user) return first;
    }
    return begin(other,layout);
}
bool xl6TreeEqualsV1(const xvalue* left,const xvalue* right,const XL6_TreeLayoutV2* layout,bool* output)
{
    xl6_error_scope scope={0}; scopeBegin(&scope);
    TreeOwner* a=begin(left,layout); TreeOwner* b=a ? beginOther(right,layout,a) : NULL;
    bool success=false,equal=false,started=false;
    xtypedtreeiter iterator={0};
    if (!a || !b || !output || !compatiblePolicies(a,b)) goto done;
    if (a==b) { success=true; equal=true; goto done; }
    if (xrtTypedTreeCount(a->storage)!=xrtTypedTreeCount(b->storage)) { success=true; goto done; }
    if (!xrtTypedTreeIterBegin(a->storage,&iterator)) goto done;
    started=true; equal=true; success=true;
    const void* key=NULL; const void* item;
    while ((item=xrtTypedTreeIterNext(&iterator,&key))!=NULL) {
        const void* peer=xrtTypedTreeConstGet(b->storage,key);
        if (!peer) { equal=false; if (xrtGetError()) success=false; break; }
        if (layout->value.transfer==3) {
            equal=xl6SequenceReferenceEqualV1(*(xvalue* const*)item,*(xvalue* const*)peer);
        } else {
            int order=0;
            if (!xrtTypeCompareValue(layout->value.type,item,peer,&order)) { success=false; break; }
            equal=order==0;
        }
        if (xrtGetError()) { success=false; break; }
        if (!equal) break;
    }
done:
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (started) xrtTypedTreeIterEnd(&iterator);
    if (b && b!=a) end(b);
    success=finish(&scope,a,true);
    if (success) *output=equal;
    return success;
}
bool xl6TreeMergeV1(const xvalue* target,const xvalue* source,const XL6_TreeLayoutV2* layout,bool replace)
{
    xl6_error_scope scope={0}; scopeBegin(&scope);
    TreeOwner* a=begin(target,layout); TreeOwner* b=a ? beginOther(source,layout,a) : NULL;
    bool success=a && b && compatiblePolicies(a,b) && xrtTypedTreeMerge(a->storage,b->storage,replace);
    if (!success || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (b && b!=a) end(b);
    return finish(&scope,a,true);
}
bool xl6TreeClearV1(const xvalue* value,const XL6_TreeLayoutV2* layout)
{
    xl6_error_scope scope={0}; scopeBegin(&scope); TreeOwner* owner=begin(value,layout);
    return finish(&scope,owner,owner && xrtTypedTreeClear(owner->storage));
}
int64_t xl6TreeCountV1(const xvalue* value,const XL6_TreeLayoutV2* layout)
{
    xl6_error_scope scope={0}; scopeBegin(&scope); TreeOwner* owner=begin(value,layout);
    size_t count=owner ? xrtTypedTreeCount(owner->storage) : 0;
    if (count>INT64_MAX) xrtSetErrorKind(XERR_RANGE);
    return finish(&scope,owner,owner && count<=INT64_MAX) ? (int64_t)count : -1;
}
int64_t xl6TreeTrimV1(const xvalue* value,const XL6_TreeLayoutV2* layout,int64_t retainEmpty)
{
    xl6_error_scope scope={0}; scopeBegin(&scope); TreeOwner* owner=begin(value,layout);
    bool success=owner && retainEmpty>=0 && (uint64_t)retainEmpty<=SIZE_MAX;
    size_t count=success ? xrtTypedTreeTrim(owner->storage,(size_t)retainEmpty) : 0;
    if ((owner && !success) || count>INT64_MAX) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_RANGE);
        success=false;
    }
    return finish(&scope,owner,success) ? (int64_t)count : -1;
}
xvalue* xl6TreeCloneV1(const xvalue* value,const XL6_TreeLayoutV2* layout)
{
    xl6_error_scope scope={0}; scopeBegin(&scope); TreeOwner* owner=begin(value,layout);
    XL6_SequenceAdmissionV3 key={0},item={0}; xvalue* result=NULL;
    if (owner && owner->keyPolicy.ops->Retain(owner->keyPolicy.data)) {
        key=owner->keyPolicy;
        if (owner->valuePolicy.ops->Retain(owner->valuePolicy.data)) {
            item=owner->valuePolicy;
            xtypedtree* clone=xrtTypedTreeClone(owner->storage);
            if (clone) result=wrap(clone,layout,&key,&item);
        }
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6SequenceAdmissionDropV3(&item); xl6SequenceAdmissionDropV3(&key);
    if (!finish(&scope,owner,true)) { xrtValueRelease(result); return NULL; }
    return result;
}
xvalue* xl6TreeProjectTakeV1(const xvalue* value,const XL6_TreeLayoutV2* layout,bool keys,
    xvalue* seed,const xvalueconverter* converter)
{
    xl6_error_scope scope={0};
    if (!(seed ? xl6ErrorScopeBegin(&scope) : xl6ErrorScopeBeginCleanup(&scope,true))) abort();
    TreeOwner* owner=seed ? begin(value,layout) : NULL; xvalue* result=NULL;
    if (owner) {
        const XL6_SequenceLayoutV1* projection=keys ? &owner->layout.key : &owner->layout.value;
        const XL6_SequenceAdmissionV3* policy=keys ? &owner->keyPolicy : &owner->valuePolicy;
        if (xl6SequenceAdmitsSchemaV1(seed,XL6_SEQUENCE_ARRAY_V1,projection,policy)) {
            result=xl6TypedTreeProjectTakeV2(owner->storage,keys,seed,projection->identity,
                projection->elementIdentity,projection->transfer,projection->size,projection->type,converter,
                keys ? owner->layout.keyCodec : NULL);
            seed=NULL;
        } else xrtSetErrorKind(XERR_TYPE);
    }
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xrtValueRelease(seed);
    if (!finish(&scope,owner,true)) { xrtValueRelease(result); return NULL; }
    return result;
}
