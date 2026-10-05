#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_ownership_scope.h"
#include "src/runtime/xl6_nullable_string.h"
#include "src/runtime/xl6_string_type.h"
#include <limits.h>
#include <string.h>

static void mutationBegin(xrtownershipscope* scope)
{ if (!xrtOwnershipMutationBegin(scope)) abort(); }
static void mutationEnd(xrtownershipscope* scope)
{ if (!xrtOwnershipScopeEnd(scope)) abort(); }

/* xrtTypeValue copies dynamic COW shells. Language class/collection carriers
 * instead copy a reference, as fixed by the XLIR transfer ABI. Keep that
 * ownership distinction in one typed slot descriptor, never in a second
 * vector/allocator implementation or a false total-order comparator. */
static bool valueInit(ptr slot, const xrttype* type)
{ (void)type; *(xvalue**)slot = NULL; return true; }
static bool valueCopy(ptr target, const void* source, const xrttype* type)
{
    (void)type;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    xvalue* item = *(xvalue* const*)source;
    xvalue* copy = item ? xrtValueRetain(item) : NULL;
    if (item && !copy) { mutationEnd(&mutation); return false; }
    xvalue* prior = *(xvalue**)target; *(xvalue**)target = copy;
    mutationEnd(&mutation);
    xrtValueRelease(prior); return true;
}
static bool valueMove(ptr target, ptr source, const xrttype* type)
{
    (void)type;
    if (target == source) return true;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    xvalue* prior = *(xvalue**)target;
    *(xvalue**)target = *(xvalue**)source; *(xvalue**)source = NULL;
    mutationEnd(&mutation);
    xrtValueRelease(prior); return true;
}
static void valueDrop(ptr slot, const xrttype* type)
{
    (void)type;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    xvalue* prior = *(xvalue**)slot; *(xvalue**)slot = NULL;
    mutationEnd(&mutation); xrtValueRelease(prior);
}
static const xrttypeops referenceOps = {
    .Init=valueInit,.Copy=valueCopy,.Move=valueMove,.Drop=valueDrop,.Clone=valueCopy
};
static const xrttype referenceType = {
    /* FNV-1a of AbiName, identical to xrtTypeId(). */
    .Id=UINT64_C(0x3438DB3BD19344B4),.Kind=XRT_TYPE_HANDLE,
    .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_REFERENCE|XRT_TYPE_FLAG_NULLABLE|
           XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
    .Name=XRT_STR_INIT("xlang-reference-slot"),.AbiName=XRT_STR_INIT("xl6.reference-slot.v1"),
    .Size=sizeof(xvalue*),.Align=_Alignof(xvalue*),.InstanceAlign=1,.Ops=&referenceOps
};

struct XL6_Sequence {
    volatile int32_t refs;
    xtypedarray* storage;
    uint64_t identity, elementIdentity;
    unsigned int transfer;
    size_t size;
    const xrttype* type;
    XL6_SequenceRoleV1 role;
    bool projectionBound;
    const xvalueconverter* projection;
    XL6_SequenceAdmissionV2 admission;
    xatomic32 busy;
    const void* claim;
    xtypedarray* retired;
    bool cleared;
    XL6_SequenceIteratorStateV1 iterator;
};

static bool allocation(bool ok)
{
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    return ok;
}
static void destroyStorage(xtypedarray* storage)
{
    xerror* saved = xrtTakeError();
    xrtTypedArrayDestroy(storage); xrtClearError(); xrtSetErrorTake(saved);
}
static void drop(void* opaque, void* user)
{
    (void)user;
    XL6_Sequence* owner = opaque;
    if (!owner) return;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    int32_t refs = xrtRefRelease(&owner->refs);
    if (refs < 0) abort();
    if (refs != 0) { mutationEnd(&mutation); return; }
    if (owner->retired || xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    xerror* saved = xrtTakeError();
    /* Detach first: destruction may invoke language object/allocator hooks. */
    xtypedarray* storage = owner->storage; owner->storage = NULL;
    mutationEnd(&mutation);
    xrtTypedArrayDestroy(storage); xl6SequenceAdmissionDropV2(&owner->admission); xrtFree(owner);
    xrtClearError(); xrtSetErrorTake(saved);
}
static bool retain(void* opaque, void** result, void* user)
{
    (void)user;
    XL6_Sequence* owner = opaque;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    bool ok = owner && result && !owner->cleared && xrtRefRetain(&owner->refs) > 0;
    if (ok) *result = owner;
    else xrtSetErrorKind(XERR_STATE);
    mutationEnd(&mutation); return ok;
}
static const xvaluehandleops ops = {retain,drop,NULL,NULL};
/* A destructor/allocator callback may release the caller's last handle.
 * Pin and publish BUSY in one participating transition. BUSY nodes REFUSE
 * graph inspection before any storage read, just like XRT's busy Value
 * backing; do not hold a mutation throughout native/user callback execution.
 * Reference slot descriptors separately protect every owning-edge transfer.
 * The atomic gate rejects concurrent/reentrant access, not a thread-safe
 * collection guarantee or automatic retry. */
static bool beginBody(XL6_Sequence* owner)
{
    uint32 expected = 0;
    if (owner->cleared || !xrtAtomic32CompareExchange(&owner->busy,&expected,1,
            XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    if (xrtRefRetain(&owner->refs) <= 0) {
        xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE);
        xrtSetErrorKind(XERR_STATE); return false;
    }
    return true;
}
void xl6SequenceEndV1(XL6_Sequence* owner)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    xrtAtomic32Store(&owner->busy,0,XMEMORY_RELEASE);
    mutationEnd(&mutation); drop(owner,NULL);
}
XL6_SequenceIteratorStateV1* xl6SequenceIteratorStateV1(XL6_Sequence* owner)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    return &owner->iterator;
}
static bool pinned(const XL6_Sequence* owner)
{ return owner && xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE) && !owner->cleared && owner->storage; }
static bool policyValid(const XL6_SequenceAdmissionV2* policy)
{
    return policy && ((!policy->ops && !policy->data && !policy->elementIdentity && !policy->transfer &&
        !policy->size && !policy->type && !policy->boxedIdentity) || (policy->ops && policy->data && policy->elementIdentity &&
        policy->transfer>=1 && policy->transfer<=3 && policy->size && policy->type &&
        ((policy->transfer==3)==(!policy->boxedIdentity)) &&
        policy->ops->Retain && policy->ops->Drop && policy->ops->Slot && policy->ops->Value &&
        policy->ops->Equivalent && policy->ops->Defaultable));
}
void xl6SequenceAdmissionDropV2(XL6_SequenceAdmissionV2* policy)
{
    if (!policy || !policyValid(policy)) abort();
    XL6_SequenceAdmissionV2 saved=*policy; *policy=(XL6_SequenceAdmissionV2){0};
    if (saved.ops) {
        xerror* error=xrtTakeError(); saved.ops->Drop(saved.data);
        xrtClearError(); xrtSetErrorTake(error);
    }
}
bool xl6SequenceAdmissionCopyV2(XL6_Sequence* owner, XL6_SequenceAdmissionV2* target)
{
    if (!pinned(owner) || !target || !policyValid(target) || target->ops) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (owner->admission.ops && !owner->admission.ops->Retain(owner->admission.data)) return false;
    *target=owner->admission; return true;
}
bool xl6SequenceAdmissionTakeV2(XL6_Sequence* owner, XL6_SequenceAdmissionV2* source)
{
    if (!policyValid(source)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!pinned(owner) || (owner->role!=XL6_SEQUENCE_ARRAY_V1 && owner->role!=XL6_SEQUENCE_CURSOR_V1) ||
        owner->admission.ops || owner->admission.data || (source->ops &&
        (source->elementIdentity!=owner->elementIdentity || source->transfer!=owner->transfer ||
         source->size!=owner->size || source->type!=owner->type))) {
        xrtSetErrorKind(XERR_ARGUMENT); xl6SequenceAdmissionDropV2(source); return false;
    }
    /* Cloned storage is allowed, but every already-owned reference must
     * satisfy the policy before it becomes visible on this owner. */
    if (source->ops) for (size_t i=0;i<xrtTypedArrayCount(owner->storage);++i)
        if (!source->ops->Slot(source->data,xrtTypedArrayConstGet(owner->storage,i))) {
            xrtSetErrorKind(XERR_TYPE); xl6SequenceAdmissionDropV2(source); return false;
        }
    owner->admission=*source; *source=(XL6_SequenceAdmissionV2){0}; return true;
}
bool xl6SequenceAdmitSlotV1(XL6_Sequence* owner, const void* slot)
{
    if (!pinned(owner) || !slot) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (owner->admission.ops && !owner->admission.ops->Slot(owner->admission.data,slot)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    return true;
}
bool xl6SequenceAdmitValueV1(XL6_Sequence* owner, const xvalue* value)
{
    if (!pinned(owner) || !value) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (owner->admission.ops && !owner->admission.ops->Value(owner->admission.data,value)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    return true;
}
bool xl6SequenceAdmissionEquivalentV1(XL6_Sequence* left, XL6_Sequence* right)
{
    if (!pinned(left) || !pinned(right)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (left->admission.ops!=right->admission.ops ||
        left->admission.boxedIdentity!=right->admission.boxedIdentity ||
        (left->admission.ops && !left->admission.ops->Equivalent(left->admission.data,right->admission.data))) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    return true;
}
uint64_t xl6SequenceBoxedIdentityV1(XL6_Sequence* owner)
{
    if (!pinned(owner)) abort();
    return owner->admission.boxedIdentity;
}
bool xl6SequenceAdmissionDefaultableV1(XL6_Sequence* owner)
{
    if (!pinned(owner)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (owner->admission.ops && !owner->admission.ops->Defaultable(owner->admission.data)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    return true;
}
static bool initDefault(ptr slot,const xrttype* type,ptr context)
{
    const XL6_SequenceAdmissionV2* policy=context;
    if (!policy->ops->InitDefault(policy->data,slot)) return false;
    if (policy->ops->Slot(policy->data,slot)) return true;
    xrtSetErrorKind(XERR_TYPE);
    xerror* error=xrtTakeError(); xrtTypeDropValue(type,slot);
    xrtClearError(); xrtSetErrorTake(error); return false;
}
bool xl6SequenceResizeStorageV1(XL6_Sequence* owner,size_t count)
{
    if (!pinned(owner)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (count>xrtTypedArrayCount(owner->storage) && !xl6SequenceAdmissionDefaultableV1(owner)) return false;
    const XL6_SequenceAdmissionV2* policy=&owner->admission;
    return xrtTypedArrayResizeWithInitializer(owner->storage,count,
        policy->ops && policy->ops->InitDefault ? initDefault : NULL,(ptr)policy);
}
bool xl6SequenceExtractTailV1(XL6_Sequence* source, XL6_Sequence* target,
    size_t depth, size_t maximum, bool take)
{
    if (!pinned(source) || !pinned(target) || source==target ||
        (source->role!=XL6_SEQUENCE_ARRAY_V1 && source->role!=XL6_SEQUENCE_STACK_V1) ||
        target->role!=XL6_SEQUENCE_ARRAY_V1 || source->iterator.prepared || target->iterator.prepared ||
        source->elementIdentity!=target->elementIdentity || source->transfer!=target->transfer ||
        source->size!=target->size || source->type!=target->type ||
        xrtTypedArrayCount(target->storage)!=0 || (take && depth)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    size_t count=xrtTypedArrayCount(source->storage);
    if (depth>count) { xrtSetErrorKind(XERR_RANGE); return false; }
    size_t available=count-depth, selected=maximum<available ? maximum : available;
    /* Stack's native payload contract does not by itself prove an Array's
     * logical domain. Check every selected slot against the final target's
     * resident policy before copying or moving anything. */
    for (size_t i=0;i<selected;++i)
        if (!xl6SequenceAdmitSlotV1(target,xrtTypedArrayConstGet(source->storage,count-depth-1-i)))
            return false;
    xtypedarray* prepared=take ? xrtTypedArrayTakeTail(source->storage,selected,true) :
        xrtTypedArraySlice(source->storage,available-selected,selected,true);
    if (!prepared) { allocation(false); return false; }
    /* TakeTail has already committed the source. The remaining publication
     * must not allocate, bind identities, retain metadata or run element
     * callbacks. Both BUSY owner pins protect the graph until this swap. */
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    xtypedarray* previous=target->storage; target->storage=prepared;
    mutationEnd(&mutation);
    /* The detached target storage was empty: no element destructor runs here.
     * Preserve the operation's pending diagnostic during allocator cleanup. */
    destroyStorage(previous); return true;
}
bool xl6SequenceTupleV1(uint64_t identity, uint64_t elementIdentity, unsigned int transfer,
                  size_t size, const xrttype* type)
{
    if (!identity || !elementIdentity || !size || !type || type->Size != size ||
        transfer < 1 || transfer > 3 || !xrtTypeIsCopyable(type) ||
        (transfer == 1 && (type->Flags & (XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP)) !=
                          (XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP)) ||
        (transfer == 2 && type != xrtTypeStringView() && !xl6NullableStringTypeAbiV1(type) &&
                         !xl6StringTypeAbiV1(type)) ||
        (transfer == 3 && type != xrtTypeValue())) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return true;
}
static XL6_Sequence* borrow(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                         unsigned int transfer, size_t size, const xrttype* type)
{
    if (!value || xrtValueType(value) == XVALUE_NULL) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    void* raw = NULL; const xvaluehandleops* actual = NULL;
    if (!xl6SequenceTupleV1(identity,elementIdentity,transfer,size,type) ||
        xrtValueTypeId(value) != identity || !xrtValueGetHandle(value,&raw,&actual,NULL) ||
        actual != &ops || !raw) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_Sequence* owner = raw;
    if (owner->cleared) { xrtSetErrorKind(XERR_STATE); return NULL; }
    if (owner->identity != identity || owner->elementIdentity != elementIdentity ||
        owner->transfer != transfer || owner->size != size || owner->type != type || !owner->storage) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    return owner;
}
static XL6_Sequence* beginAs(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                        unsigned int transfer, size_t size, const xrttype* type, bool iterator)
{
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    XL6_Sequence* owner = borrow(value,identity,elementIdentity,transfer,size,type);
    if (owner && (owner->iterator.prepared != iterator ||
        (iterator && owner->role!=XL6_SEQUENCE_CURSOR_V1))) { xrtSetErrorKind(XERR_ARGUMENT); owner=NULL; }
    if (owner && !beginBody(owner)) owner = NULL;
    mutationEnd(&mutation); return owner;
}
XL6_Sequence* xl6SequenceBeginV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                        unsigned int transfer, size_t size, const xrttype* type)
{ return beginAs(value,identity,elementIdentity,transfer,size,type,false); }
XL6_Sequence* xl6SequenceBeginRoleV1(const xvalue* value, XL6_SequenceRoleV1 role,
    uint64_t identity, uint64_t elementIdentity, unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Sequence* owner=beginAs(value,identity,elementIdentity,transfer,size,type,false);
    if (owner && owner->role!=role) {
        xl6SequenceEndV1(owner); xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    return owner;
}
bool xl6SequenceBindProjectionV1(XL6_Sequence* owner, const xvalueconverter* converter)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE) ||
        owner->role!=XL6_SEQUENCE_ARRAY_V1 || owner->cleared ||
        (converter && (converter->Context || !converter->ToTyped || !converter->FromTyped)) ||
        (owner->projectionBound && owner->projection!=converter)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    owner->projection=converter; owner->projectionBound=true; return true;
}
int xl6SequenceBeginErasedArrayV1(const xvalue* value, XL6_Sequence** result,
    XL6_SequenceLayoutV1* layout, const xvalueconverter** converter)
{
    if (!result || !layout || !converter) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    /* No foreign pointer is dereferenced before the sealed handle ops match. */
    xrtownershipscope mutation={0}; mutationBegin(&mutation);
    if (!value || xrtValueType(value)!=XVALUE_HANDLE) { mutationEnd(&mutation); return 0; }
    void* raw=NULL; const xvaluehandleops* actual=NULL;
    if (!xrtValueGetHandle(value,&raw,&actual,NULL)) { mutationEnd(&mutation); return -1; }
    if (actual!=&ops || !raw) { mutationEnd(&mutation); return 0; }
    XL6_Sequence* owner=raw; int status=0; bool unbound=false;
    if (owner->role==XL6_SEQUENCE_ARRAY_V1) {
        status=-1;
        if (owner->cleared || !owner->storage ||
            xrtValueTypeId(value)!=owner->identity) xrtSetErrorKind(XERR_STATE);
        else if (beginBody(owner)) {
            /* Read the policy only AFTER acquiring BUSY. Binding publishes it
             * through End's release; a concurrent binder must not race with
             * an early read of projectionBound/projection. */
            if (!owner->projectionBound) unbound=true;
            else {
                *layout=(XL6_SequenceLayoutV1){owner->identity,owner->elementIdentity,
                    owner->transfer,owner->size,owner->type};
                *converter=owner->projection; *result=owner; status=1;
            }
        }
    }
    mutationEnd(&mutation);
    if (unbound) { xl6SequenceEndV1(owner); xrtSetErrorKind(XERR_STATE); }
    return status;
}
XL6_Sequence* xl6SequenceIteratorBeginV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                        unsigned int transfer, size_t size, const xrttype* type)
{ return beginAs(value,identity,elementIdentity,transfer,size,type,true); }

/* Physical graph: each Value shell owns ONE wrapper, and the wrapper owns
 * each native reference slot once (including duplicates). Never flatten the
 * shell directly to elements: two shells can share this counted wrapper. */
static bool ownershipCount(const void* data, size_t* count)
{
    const XL6_Sequence* owner = data;
    if (!owner || !count || owner->refs <= 0 || owner->cleared ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) return false;
    *count = (size_t)owner->refs; return true;
}
static bool ownershipTrace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_Sequence* owner = data; size_t refs;
    if (!visit || !ownershipCount(data,&refs) || !owner->storage) return false;
    if (owner->transfer != 3) return true;
    size_t count = xrtTypedArrayCount(owner->storage);
    for (size_t depth = 0; depth < count; ++depth) {
        const xvalue* item = *(xvalue* const*)xrtTypedArrayConstGet(owner->storage,count-depth-1u);
        if (!visit(xrtValueOwnership(item),context)) return false;
    }
    return true;
}
static const xrtownershipops ownershipOps = {ownershipCount,ownershipTrace};
static bool handleTrace(const xvalue* value, xrtownershipvisitor visit, void* context)
{
    void* raw = NULL; const xvaluehandleops* actual = NULL; void* user = NULL;
    return visit && xrtValueGetHandle(value,&raw,&actual,&user) && actual == &ops && !user && raw &&
        visit((xrtownershipref){raw,&ownershipOps},context);
}
static bool ownershipHold(const void* data)
{
    XL6_Sequence* owner = (XL6_Sequence*)data;
    return owner->refs > 0 && owner->refs < INT32_MAX && xrtRefRetain(&owner->refs) > 0;
}
static void ownershipDrop(const void* data) { drop((void*)data,NULL); }
static bool ownershipClaim(const void* data, const void* token)
{
    XL6_Sequence* owner = (XL6_Sequence*)data;
    if (!token || owner->cleared || (owner->claim && owner->claim != token)) return false;
    owner->claim = token; return true;
}
static void ownershipRestore(const void* data, const void* token)
{
    XL6_Sequence* owner = (XL6_Sequence*)data;
    if (!token || owner->claim != token || owner->cleared) abort();
    owner->claim = NULL;
}
static void ownershipClear(const void* data, const void* token)
{
    XL6_Sequence* owner = (XL6_Sequence*)data;
    if (!token || owner->claim != token || owner->cleared || owner->retired ||
        xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE)) abort();
    /* Freeze phase is infallible: no allocation, element drop or user code. */
    owner->retired = owner->storage; owner->storage = NULL; owner->cleared = true;
}
static bool ownershipFinish(const void* data, const void* token)
{
    XL6_Sequence* owner = (XL6_Sequence*)data;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    if (!token || owner->claim != token || !owner->cleared) abort();
    xtypedarray* retired = owner->retired; owner->retired = NULL;
    mutationEnd(&mutation); destroyStorage(retired); return true;
}
const xrtownershipadapterv1* xl6SequenceOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter),ownershipHold,ownershipDrop,
        ownershipClaim,ownershipRestore,NULL,ownershipClear,ownershipFinish};
    size_t refs;
    if (ref.Data && ref.Ops == &ownershipOps)
        return ownershipCount(ref.Data,&refs) ? &adapter : NULL;
    return xrtValueHandleOwnershipAdapterV1(ref,&ops,handleTrace);
}
xvalue* xl6SequenceWrapAsV1(xtypedarray* storage, XL6_SequenceRoleV1 role, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type)
{
    if (!storage) { allocation(false); return NULL; }
    if ((role!=XL6_SEQUENCE_ARRAY_V1 && role!=XL6_SEQUENCE_STACK_V1 && role!=XL6_SEQUENCE_CURSOR_V1) ||
        !xl6SequenceTupleV1(identity,elementIdentity,transfer,size,type)) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT);
        destroyStorage(storage); return NULL;
    }
    if (xrtTypedArrayItemType(storage) != xl6SequenceStorageTypeV1(transfer,type)) {
        xrtSetErrorKind(XERR_ARGUMENT); destroyStorage(storage); return NULL;
    }
    XL6_Sequence* owner = xrtMalloc(sizeof(*owner));
    if (!owner) { allocation(false); destroyStorage(storage); return NULL; }
    *owner = (XL6_Sequence){.refs=1,.storage=storage,.identity=identity,
        .elementIdentity=elementIdentity,.transfer=transfer,.size=size,.type=type,.role=role};
    xrtAtomic32Init(&owner->busy,0);
    xvalue* result = xrtValueHandleTake((void**)&owner,&ops,NULL);
    if (!result) { allocation(false); drop(owner,NULL); return NULL; }
    if (!xrtValueHandleOwnershipBindPhased(result,handleTrace) || !xrtValueTypeIdBind(result,identity)) {
        xerror* failure = xrtTakeError(); xrtValueRelease(result);
        xrtClearError(); xrtSetErrorTake(failure); return NULL;
    }
    return result;
}
xvalue* xl6SequenceWrapV1(xtypedarray* storage, uint64_t identity, uint64_t elementIdentity,
    unsigned int transfer, size_t size, const xrttype* type)
{ return xl6SequenceWrapAsV1(storage,XL6_SEQUENCE_ARRAY_V1,identity,elementIdentity,transfer,size,type); }
bool xl6SequenceEqualsRoleV1(const xvalue* left, const xvalue* right, XL6_SequenceRoleV1 role, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Sequence* a = xl6SequenceBeginRoleV1(left,role,identity,elementIdentity,transfer,size,type);
    if (!a) return false;
    XL6_Sequence* b = xl6SequenceBeginOtherV1(right,identity,elementIdentity,transfer,size,type,a);
    if (!b) { xl6SequenceEndV1(a); return false; }
    bool equal = xl6SequenceAdmissionEquivalentV1(a,b);
    if (!equal) goto done;
    if (transfer != 3) {
        equal = xrtTypedArrayEquals(a->storage,b->storage);
        goto done;
    }
    /* Language managed carriers have equality but no total ordering. Their
     * xrttype intentionally has no Compare; delegate to XRT value equality,
     * not a made-up ordering or comparison of physical pointer bytes. */
    size_t count = xrtTypedArrayCount(a->storage);
    if (count != xrtTypedArrayCount(b->storage)) { equal = false; goto done; }
    for (size_t depth=0; depth<count; ++depth) {
        const xvalue* x = *(xvalue* const*)xrtTypedArrayConstGet(a->storage,count-depth-1u);
        const xvalue* y = *(xvalue* const*)xrtTypedArrayConstGet(b->storage,count-depth-1u);
        if (!xl6SequenceReferenceEqualV1(x,y)) { equal = false; break; }
    }
done:
    if (b != a) xl6SequenceEndV1(b);
    xl6SequenceEndV1(a); return equal;
}
bool xl6SequenceEqualsV1(const xvalue* left, const xvalue* right, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type)
{ return xl6SequenceEqualsRoleV1(left,right,XL6_SEQUENCE_ARRAY_V1,identity,elementIdentity,transfer,size,type); }

const xrttype* xl6SequenceStorageTypeV1(unsigned int transfer, const xrttype* type)
{ return transfer == 3 ? &referenceType : type; }
XL6_Sequence* xl6SequenceBeginOtherV1(const xvalue* value, uint64_t identity,
    uint64_t elementIdentity, unsigned int transfer, size_t size,
    const xrttype* type, XL6_Sequence* first)
{
    if (!first || first->iterator.prepared || !xrtAtomic32Load(&first->busy,XMEMORY_ACQUIRE)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    XL6_Sequence* other = borrow(value,identity,elementIdentity,transfer,size,type);
    if (other && (other->iterator.prepared || other->role!=first->role)) { xrtSetErrorKind(XERR_ARGUMENT); other=NULL; }
    if (other && other != first && !beginBody(other)) other = NULL;
    mutationEnd(&mutation); return other;
}
xtypedarray* xl6SequenceStorageV1(XL6_Sequence* owner)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE) || owner->cleared)
        abort();
    return owner->storage;
}
