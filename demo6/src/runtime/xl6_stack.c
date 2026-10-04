#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_ownership_scope.h"
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

typedef struct XL6_Stack {
    volatile int32_t refs;
    xtypedstack* storage;
    uint64_t identity, elementIdentity;
    unsigned int transfer;
    size_t size;
    const xrttype* type;
    xatomic32 busy;
    const void* claim;
    xtypedstack* retired;
    bool cleared;
} XL6_Stack;

static bool allocation(bool ok)
{
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    return ok;
}
static void destroyStorage(xtypedstack* storage)
{
    xerror* saved = xrtTakeError();
    xrtTypedStackDestroy(storage); xrtClearError(); xrtSetErrorTake(saved);
}
static void drop(void* opaque, void* user)
{
    (void)user;
    XL6_Stack* stack = opaque;
    if (!stack) return;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    int32_t refs = xrtRefRelease(&stack->refs);
    if (refs < 0) abort();
    if (refs != 0) { mutationEnd(&mutation); return; }
    if (stack->retired || xrtAtomic32Load(&stack->busy,XMEMORY_ACQUIRE)) abort();
    xerror* saved = xrtTakeError();
    /* Detach first: destruction may invoke language object/allocator hooks. */
    xtypedstack* storage = stack->storage; stack->storage = NULL;
    mutationEnd(&mutation);
    xrtTypedStackDestroy(storage); xrtFree(stack);
    xrtClearError(); xrtSetErrorTake(saved);
}
static bool retain(void* opaque, void** result, void* user)
{
    (void)user;
    XL6_Stack* stack = opaque;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    bool ok = stack && result && !stack->cleared && xrtRefRetain(&stack->refs) > 0;
    if (ok) *result = stack;
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
static bool beginBody(XL6_Stack* stack)
{
    uint32 expected = 0;
    if (stack->cleared || !xrtAtomic32CompareExchange(&stack->busy,&expected,1,
            XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    if (xrtRefRetain(&stack->refs) <= 0) {
        xrtAtomic32Store(&stack->busy,0,XMEMORY_RELEASE);
        xrtSetErrorKind(XERR_STATE); return false;
    }
    return true;
}
static void end(XL6_Stack* stack)
{
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    xrtAtomic32Store(&stack->busy,0,XMEMORY_RELEASE);
    mutationEnd(&mutation); drop(stack,NULL);
}
static bool tuple(uint64_t identity, uint64_t elementIdentity, unsigned int transfer,
                  size_t size, const xrttype* type)
{
    if (!identity || !elementIdentity || !size || !type || type->Size != size ||
        transfer < 1 || transfer > 3 || !xrtTypeIsCopyable(type) ||
        (transfer == 1 && (type->Flags & (XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP)) !=
                          (XRT_TYPE_FLAG_TRIVIAL_COPY|XRT_TYPE_FLAG_TRIVIAL_DROP)) ||
        (transfer == 2 && type != xrtTypeStringView()) ||
        (transfer == 3 && type != xrtTypeValue())) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    return true;
}
static XL6_Stack* borrow(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                         unsigned int transfer, size_t size, const xrttype* type)
{
    if (!value || xrtValueType(value) == XVALUE_NULL) return NULL;
    void* raw = NULL; const xvaluehandleops* actual = NULL;
    if (!tuple(identity,elementIdentity,transfer,size,type) ||
        xrtValueTypeId(value) != identity || !xrtValueGetHandle(value,&raw,&actual,NULL) ||
        actual != &ops || !raw) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_Stack* stack = raw;
    if (stack->cleared) { xrtSetErrorKind(XERR_STATE); return NULL; }
    if (stack->identity != identity || stack->elementIdentity != elementIdentity ||
        stack->transfer != transfer || stack->size != size || stack->type != type || !stack->storage) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    return stack;
}
static XL6_Stack* begin(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                        unsigned int transfer, size_t size, const xrttype* type)
{
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (stack && !beginBody(stack)) stack = NULL;
    mutationEnd(&mutation); return stack;
}

/* Physical graph: each Value shell owns ONE wrapper, and the wrapper owns
 * each native reference slot once (including duplicates). Never flatten the
 * shell directly to elements: two shells can share this counted wrapper. */
static bool ownershipCount(const void* data, size_t* count)
{
    const XL6_Stack* stack = data;
    if (!stack || !count || stack->refs <= 0 || stack->cleared ||
        xrtAtomic32Load(&stack->busy,XMEMORY_ACQUIRE)) return false;
    *count = (size_t)stack->refs; return true;
}
static bool ownershipTrace(const void* data, xrtownershipvisitor visit, void* context)
{
    const XL6_Stack* stack = data; size_t refs;
    if (!visit || !ownershipCount(data,&refs) || !stack->storage) return false;
    if (stack->transfer != 3) return true;
    size_t count = xrtTypedStackCount(stack->storage);
    for (size_t depth = 0; depth < count; ++depth) {
        const xvalue* item = *(xvalue* const*)xrtTypedStackConstPeek(stack->storage,depth);
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
    XL6_Stack* stack = (XL6_Stack*)data;
    return stack->refs > 0 && stack->refs < INT32_MAX && xrtRefRetain(&stack->refs) > 0;
}
static void ownershipDrop(const void* data) { drop((void*)data,NULL); }
static bool ownershipClaim(const void* data, const void* token)
{
    XL6_Stack* stack = (XL6_Stack*)data;
    if (!token || stack->cleared || (stack->claim && stack->claim != token)) return false;
    stack->claim = token; return true;
}
static void ownershipRestore(const void* data, const void* token)
{
    XL6_Stack* stack = (XL6_Stack*)data;
    if (!token || stack->claim != token || stack->cleared) abort();
    stack->claim = NULL;
}
static void ownershipClear(const void* data, const void* token)
{
    XL6_Stack* stack = (XL6_Stack*)data;
    if (!token || stack->claim != token || stack->cleared || stack->retired ||
        xrtAtomic32Load(&stack->busy,XMEMORY_ACQUIRE)) abort();
    /* Freeze phase is infallible: no allocation, element drop or user code. */
    stack->retired = stack->storage; stack->storage = NULL; stack->cleared = true;
}
static bool ownershipFinish(const void* data, const void* token)
{
    XL6_Stack* stack = (XL6_Stack*)data;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    if (!token || stack->claim != token || !stack->cleared) abort();
    xtypedstack* retired = stack->retired; stack->retired = NULL;
    mutationEnd(&mutation); destroyStorage(retired); return true;
}
const xrtownershipadapterv1* xl6StackOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter),ownershipHold,ownershipDrop,
        ownershipClaim,ownershipRestore,NULL,ownershipClear,ownershipFinish};
    size_t refs;
    if (ref.Data && ref.Ops == &ownershipOps)
        return ownershipCount(ref.Data,&refs) ? &adapter : NULL;
    return xrtValueHandleOwnershipAdapterV1(ref,&ops,handleTrace);
}
static xvalue* wrap(xtypedstack* storage, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type)
{
    if (!storage) { allocation(false); return NULL; }
    XL6_Stack* stack = xrtMalloc(sizeof(*stack));
    if (!stack) { allocation(false); destroyStorage(storage); return NULL; }
    *stack = (XL6_Stack){.refs=1,.storage=storage,.identity=identity,
        .elementIdentity=elementIdentity,.transfer=transfer,.size=size,.type=type};
    xrtAtomic32Init(&stack->busy,0);
    xvalue* result = xrtValueHandleTake((void**)&stack,&ops,NULL);
    if (!result) { allocation(false); drop(stack,NULL); return NULL; }
    if (!xrtValueHandleOwnershipBindPhased(result,handleTrace) || !xrtValueTypeIdBind(result,identity)) {
        xerror* failure = xrtTakeError(); xrtValueRelease(result);
        xrtClearError(); xrtSetErrorTake(failure); return NULL;
    }
    return result;
}
xvalue* xl6StackCreateV1(uint64_t identity, uint64_t elementIdentity, unsigned int transfer,
                        size_t size, const xrttype* type)
{
    if (!tuple(identity,elementIdentity,transfer,size,type)) return NULL;
    return wrap(xrtTypedStackCreate(transfer == 3 ? &referenceType : type),identity,elementIdentity,transfer,size,type);
}
bool xl6StackPushV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Stack* stack = begin(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    bool ok = allocation(xrtTypedStackPush(stack->storage,item));
    end(stack); return ok;
}
bool xl6StackPopOrPeekV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                         unsigned int transfer, size_t size, const xrttype* type, void* target, bool peek)
{
    if (peek) return xl6StackPeekAtV1(value,identity,elementIdentity,transfer,size,type,0,target);
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Stack* stack = begin(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    bool ok = xrtTypedStackPop(stack->storage,target);
    end(stack); return ok;
}
bool xl6StackPeekAtV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type, int64_t depth, void* target)
{
    if (depth < 0 || (uint64_t)depth > SIZE_MAX) return false;
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Stack* stack = begin(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    const void* item = xrtTypedStackConstPeek(stack->storage,(size_t)depth);
    if (!item) { end(stack); return false; }
    bool ok = allocation(xrtTypeCopyValue(xrtTypedStackItemType(stack->storage),target,item));
    end(stack); return ok;
}
bool xl6StackReserveV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                       unsigned int transfer, size_t size, const xrttype* type, int64_t capacity)
{
    if (capacity < 0 || (uint64_t)capacity > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    XL6_Stack* stack = begin(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    bool ok = allocation(xrtTypedStackReserve(stack->storage,(size_t)capacity));
    end(stack); return ok;
}
bool xl6StackTryTakeV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                       unsigned int transfer, size_t size, const xrttype* type,
                       int64_t depth, void* target, bool pop)
{
    if (depth < 0 || (uint64_t)depth > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (pop && depth != 0) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    return pop
        ? xl6StackPopOrPeekV1(value,identity,elementIdentity,transfer,size,type,target,false)
        : xl6StackPeekAtV1(value,identity,elementIdentity,transfer,size,type,depth,target);
}
void xl6StackTakeV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type,
                    int64_t depth, void* target, bool pop)
{
    if (pop && depth != 0) { xrtSetErrorKind(XERR_ARGUMENT); return; }
    bool ok = pop
        ? xl6StackPopOrPeekV1(value,identity,elementIdentity,transfer,size,type,target,false)
        : xl6StackPeekAtV1(value,identity,elementIdentity,transfer,size,type,depth,target);
    /* Empty/out-of-range is a language failure, not a fabricated T default.
     * Never replace an allocation/retain/state error from the actual storage. */
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_RANGE);
}
xvalue* xl6StackCloneV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                       unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Stack* stack = begin(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return NULL;
    xtypedstack* copy = xrtTypedStackClone(stack->storage);
    end(stack);
    return wrap(copy,identity,elementIdentity,transfer,size,type);
}
bool xl6StackEqualsV1(const xvalue* left, const xvalue* right, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Stack* a = begin(left,identity,elementIdentity,transfer,size,type);
    if (!a) return false;
    xrtownershipscope mutation = {0}; mutationBegin(&mutation);
    XL6_Stack* b = borrow(right,identity,elementIdentity,transfer,size,type);
    if (b && b != a && !beginBody(b)) b = NULL;
    mutationEnd(&mutation);
    if (!b) { end(a); return false; }
    bool equal = true;
    if (transfer != 3) {
        equal = xrtTypedStackEquals(a->storage,b->storage);
        goto done;
    }
    /* Language managed carriers have equality but no total ordering. Their
     * xrttype intentionally has no Compare; delegate to XRT value equality,
     * not a made-up ordering or comparison of physical pointer bytes. */
    size_t count = xrtTypedStackCount(a->storage);
    if (count != xrtTypedStackCount(b->storage)) { equal = false; goto done; }
    for (size_t depth=0; depth<count; ++depth) {
        const xvalue* x = *(xvalue* const*)xrtTypedStackConstPeek(a->storage,depth);
        const xvalue* y = *(xvalue* const*)xrtTypedStackConstPeek(b->storage,depth);
        if (x != y && (!x || !y || !xrtValueEqual(x,y))) { equal = false; break; }
    }
done:
    if (b != a) end(b);
    end(a); return equal;
}
bool xl6StackTrimV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Stack* stack = begin(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    bool ok = allocation(xrtTypedStackTrim(stack->storage));
    end(stack); return ok;
}
void xl6StackClearV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                     unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Stack* stack = begin(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return;
    xerror* saved = xrtTakeError(); xrtTypedStackClear(stack->storage);
    xrtClearError(); xrtSetErrorTake(saved); end(stack);
}
int64_t xl6StackSizeV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type, bool capacity)
{
    XL6_Stack* stack = begin(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return 0;
    size_t result = capacity ? xrtTypedStackCapacity(stack->storage) : xrtTypedStackCount(stack->storage);
    end(stack);
    if (result > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)result;
}
