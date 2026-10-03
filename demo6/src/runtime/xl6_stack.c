#include "src/runtime/xl6_stack.h"
#include <limits.h>
#include <string.h>

/* xrtTypeValue copies dynamic COW shells. Language class/collection carriers
 * instead copy a reference, as fixed by the XLIR transfer ABI. Keep that
 * ownership distinction in one typed slot descriptor, never in a second
 * vector/allocator implementation or a false total-order comparator. */
static bool valueInit(ptr slot, const xrttype* type)
{ (void)type; *(xvalue**)slot = NULL; return true; }
static bool valueCopy(ptr target, const void* source, const xrttype* type)
{
    (void)type;
    xvalue* item = *(xvalue* const*)source;
    xvalue* copy = item ? xrtValueRetain(item) : NULL;
    if (item && !copy) return false;
    xvalue* prior = *(xvalue**)target; *(xvalue**)target = copy;
    xrtValueRelease(prior); return true;
}
static bool valueMove(ptr target, ptr source, const xrttype* type)
{
    (void)type;
    if (target == source) return true;
    xvalue* prior = *(xvalue**)target;
    *(xvalue**)target = *(xvalue**)source; *(xvalue**)source = NULL;
    xrtValueRelease(prior); return true;
}
static void valueDrop(ptr slot, const xrttype* type)
{ (void)type; xvalue* prior = *(xvalue**)slot; *(xvalue**)slot = NULL; xrtValueRelease(prior); }
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
    bool busy;
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
    if (xrtRefRelease(&stack->refs) != 0) return;
    xerror* saved = xrtTakeError();
    /* Detach first: destruction may invoke language object/allocator hooks. */
    xtypedstack* storage = stack->storage; stack->storage = NULL;
    xrtTypedStackDestroy(storage); xrtFree(stack);
    xrtClearError(); xrtSetErrorTake(saved);
}
static bool retain(void* opaque, void** result, void* user)
{
    (void)user;
    XL6_Stack* stack = opaque;
    if (!stack || !result || xrtRefRetain(&stack->refs) < 0) {
        xrtSetErrorKind(XERR_STATE); return false;
    }
    *result = stack; return true;
}
static const xvaluehandleops ops = {retain,drop,NULL,NULL};
/* A destructor/allocator callback may release the caller's last handle.
 * Pin the wrapper as well as rejecting reentrant access to its storage. */
static bool begin(XL6_Stack* stack)
{
    if (xrtRefRetain(&stack->refs) < 0) { xrtSetErrorKind(XERR_STATE); return false; }
    stack->busy = true; return true;
}
static void end(XL6_Stack* stack)
{ stack->busy = false; drop(stack,NULL); }
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
    if (stack->busy) { xrtSetErrorKind(XERR_STATE); return NULL; }
    if (stack->identity != identity || stack->elementIdentity != elementIdentity ||
        stack->transfer != transfer || stack->size != size || stack->type != type || !stack->storage) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    return stack;
}
static xvalue* wrap(xtypedstack* storage, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type)
{
    if (!storage) { allocation(false); return NULL; }
    XL6_Stack* stack = xrtMalloc(sizeof(*stack));
    if (!stack) { allocation(false); destroyStorage(storage); return NULL; }
    *stack = (XL6_Stack){1,storage,identity,elementIdentity,transfer,size,type,false};
    xvalue* result = xrtValueHandleTake((void**)&stack,&ops,NULL);
    if (!result) { allocation(false); drop(stack,NULL); return NULL; }
    if (!xrtValueTypeIdBind(result,identity)) {
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
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!begin(stack)) return false;
    bool ok = allocation(xrtTypedStackPush(stack->storage,item));
    end(stack); return ok;
}
bool xl6StackPopOrPeekV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                         unsigned int transfer, size_t size, const xrttype* type, void* target, bool peek)
{
    if (peek) return xl6StackPeekAtV1(value,identity,elementIdentity,transfer,size,type,0,target);
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!begin(stack)) return false;
    bool ok = xrtTypedStackPop(stack->storage,target);
    end(stack); return ok;
}
bool xl6StackPeekAtV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type, int64_t depth, void* target)
{
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (!stack || depth < 0 || (uint64_t)depth > SIZE_MAX) return false;
    const void* item = xrtTypedStackConstPeek(stack->storage,(size_t)depth);
    if (!item) return false;
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!begin(stack)) return false;
    bool ok = allocation(xrtTypeCopyValue(xrtTypedStackItemType(stack->storage),target,item));
    end(stack); return ok;
}
bool xl6StackReserveV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                       unsigned int transfer, size_t size, const xrttype* type, int64_t capacity)
{
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    if (capacity < 0 || (uint64_t)capacity > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!begin(stack)) return false;
    bool ok = allocation(xrtTypedStackReserve(stack->storage,(size_t)capacity));
    end(stack); return ok;
}
xvalue* xl6StackCloneV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                       unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return NULL;
    if (!begin(stack)) return NULL;
    xtypedstack* copy = xrtTypedStackClone(stack->storage);
    end(stack);
    return wrap(copy,identity,elementIdentity,transfer,size,type);
}
bool xl6StackEqualsV1(const xvalue* left, const xvalue* right, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Stack* a = borrow(left,identity,elementIdentity,transfer,size,type);
    XL6_Stack* b = borrow(right,identity,elementIdentity,transfer,size,type);
    if (!a || !b) return false;
    if (!begin(a)) return false;
    if (b != a && !begin(b)) { end(a); return false; }
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
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    if (!begin(stack)) return false;
    bool ok = allocation(xrtTypedStackTrim(stack->storage));
    end(stack); return ok;
}
void xl6StackClearV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                     unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return;
    if (!begin(stack)) return;
    xerror* saved = xrtTakeError(); xrtTypedStackClear(stack->storage);
    xrtClearError(); xrtSetErrorTake(saved); end(stack);
}
int64_t xl6StackSizeV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type, bool capacity)
{
    XL6_Stack* stack = borrow(value,identity,elementIdentity,transfer,size,type);
    if (!stack) return 0;
    size_t result = capacity ? xrtTypedStackCapacity(stack->storage) : xrtTypedStackCount(stack->storage);
    if (result > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)result;
}
