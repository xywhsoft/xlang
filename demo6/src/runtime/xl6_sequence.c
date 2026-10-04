#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_ownership_scope.h"
#include "src/runtime/xl6_nullable_string.h"
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
    xrtTypedArrayDestroy(storage); xrtFree(owner);
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
bool xl6SequenceTupleV1(uint64_t identity, uint64_t elementIdentity, unsigned int transfer,
                  size_t size, const xrttype* type)
{
    if (!identity || !elementIdentity || !size || !type || type->Size != size ||
        transfer < 1 || transfer > 3 || !xrtTypeIsCopyable(type) ||
        (transfer == 1 && (type->Flags & (XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP)) !=
                          (XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP)) ||
        (transfer == 2 && type != xrtTypeStringView() && !xl6NullableStringTypeAbiV1(type)) ||
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
    if (owner && owner->iterator.prepared != iterator) { xrtSetErrorKind(XERR_ARGUMENT); owner=NULL; }
    if (owner && !beginBody(owner)) owner = NULL;
    mutationEnd(&mutation); return owner;
}
XL6_Sequence* xl6SequenceBeginV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                        unsigned int transfer, size_t size, const xrttype* type)
{ return beginAs(value,identity,elementIdentity,transfer,size,type,false); }
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
xvalue* xl6SequenceWrapV1(xtypedarray* storage, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type)
{
    if (!storage) { allocation(false); return NULL; }
    if (!xl6SequenceTupleV1(identity,elementIdentity,transfer,size,type)) {
        destroyStorage(storage); return NULL;
    }
    if (xrtTypedArrayItemType(storage) != xl6SequenceStorageTypeV1(transfer,type)) {
        xrtSetErrorKind(XERR_ARGUMENT); destroyStorage(storage); return NULL;
    }
    XL6_Sequence* owner = xrtMalloc(sizeof(*owner));
    if (!owner) { allocation(false); destroyStorage(storage); return NULL; }
    *owner = (XL6_Sequence){.refs=1,.storage=storage,.identity=identity,
        .elementIdentity=elementIdentity,.transfer=transfer,.size=size,.type=type};
    xrtAtomic32Init(&owner->busy,0);
    xvalue* result = xrtValueHandleTake((void**)&owner,&ops,NULL);
    if (!result) { allocation(false); drop(owner,NULL); return NULL; }
    if (!xrtValueHandleOwnershipBindPhased(result,handleTrace) || !xrtValueTypeIdBind(result,identity)) {
        xerror* failure = xrtTakeError(); xrtValueRelease(result);
        xrtClearError(); xrtSetErrorTake(failure); return NULL;
    }
    return result;
}
bool xl6SequenceEqualsV1(const xvalue* left, const xvalue* right, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Sequence* a = xl6SequenceBeginV1(left,identity,elementIdentity,transfer,size,type);
    if (!a) return false;
    XL6_Sequence* b = xl6SequenceBeginOtherV1(right,identity,elementIdentity,transfer,size,type,a);
    if (!b) { xl6SequenceEndV1(a); return false; }
    bool equal = true;
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
    if (other && other->iterator.prepared) { xrtSetErrorKind(XERR_ARGUMENT); other=NULL; }
    if (other && other != first && !beginBody(other)) other = NULL;
    mutationEnd(&mutation); return other;
}
xtypedarray* xl6SequenceStorageV1(XL6_Sequence* owner)
{
    if (!owner || !xrtAtomic32Load(&owner->busy,XMEMORY_ACQUIRE) || owner->cleared)
        abort();
    return owner->storage;
}
