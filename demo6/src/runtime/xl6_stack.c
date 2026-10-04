#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_sequence.h"
#include <limits.h>

static bool allocation(bool ok)
{ if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return ok; }
xvalue* xl6StackCreateV1(uint64_t identity, uint64_t elementIdentity, unsigned int transfer,
                        size_t size, const xrttype* type)
{
    if (!xl6SequenceTupleV1(identity,elementIdentity,transfer,size,type)) return NULL;
    return xl6SequenceWrapAsV1(xrtTypedStackCreate(xl6SequenceStorageTypeV1(transfer,type)),XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
}
bool xl6StackPushV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* stack = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    bool ok = allocation(xrtTypedStackPush(xl6SequenceStorageV1(stack),item));
    xl6SequenceEndV1(stack); return ok;
}
bool xl6StackReserveV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                       unsigned int transfer, size_t size, const xrttype* type, int64_t capacity)
{
    if (capacity < 0 || (uint64_t)capacity > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    XL6_Sequence* stack = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    bool ok = allocation(xrtTypedStackReserve(xl6SequenceStorageV1(stack),(size_t)capacity));
    xl6SequenceEndV1(stack); return ok;
}
bool xl6StackTryTakeV2(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                       unsigned int transfer, size_t size, const xrttype* type,
                       int64_t depth, void* target, bool pop, bool* found)
{
    if (!found) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    *found = false;
    if (depth < 0 || (uint64_t)depth > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (pop && depth != 0) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* stack = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    xtypedstack* storage = xl6SequenceStorageV1(stack);
    if ((size_t)depth >= xrtTypedStackCount(storage)) {
        xl6SequenceEndV1(stack); return true;
    }
    bool ok;
    if (pop) ok = allocation(xrtTypedStackPop(storage,target));
    else {
        const void* item = xrtTypedStackConstPeek(storage,(size_t)depth);
        ok = item != NULL && allocation(xrtTypeCopyValue(xrtTypedStackItemType(storage),target,item));
    }
    *found = ok;
    xl6SequenceEndV1(stack); return ok;
}
bool xl6StackTakeV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type,
                    int64_t depth, void* target, bool pop)
{
    bool found = false;
    bool ok = xl6StackTryTakeV2(value,identity,elementIdentity,transfer,size,type,depth,target,pop,&found);
    /* Empty/out-of-range is a language failure, not a fabricated T default.
     * Never replace an allocation/retain/state error from the actual storage. */
    if (ok && !found) { xrtSetErrorKind(XERR_RANGE); return false; }
    return ok;
}
xvalue* xl6StackCloneV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                       unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Sequence* stack = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
    if (!stack) return NULL;
    xtypedstack* copy = xrtTypedStackClone(xl6SequenceStorageV1(stack));
    xl6SequenceEndV1(stack);
    return xl6SequenceWrapAsV1(copy,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
}
bool xl6StackEqualsV1(const xvalue* left, const xvalue* right, uint64_t identity,
    uint64_t elementIdentity, unsigned int transfer, size_t size, const xrttype* type)
{ return xl6SequenceEqualsRoleV1(left,right,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type); }
bool xl6StackTrimV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                    unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Sequence* stack = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    bool ok = allocation(xrtTypedStackTrim(xl6SequenceStorageV1(stack)));
    xl6SequenceEndV1(stack); return ok;
}
bool xl6StackClearV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                     unsigned int transfer, size_t size, const xrttype* type)
{
    XL6_Sequence* stack = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
    if (!stack) return false;
    xrtTypedStackClear(xl6SequenceStorageV1(stack));
    xl6SequenceEndV1(stack);
    return !xrtGetError();
}
int64_t xl6StackSizeV1(const xvalue* value, uint64_t identity, uint64_t elementIdentity,
                      unsigned int transfer, size_t size, const xrttype* type, bool capacity)
{
    XL6_Sequence* stack = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_STACK_V1,identity,elementIdentity,transfer,size,type);
    if (!stack) return 0;
    size_t result = capacity ? xrtTypedStackCapacity(xl6SequenceStorageV1(stack)) : xrtTypedStackCount(xl6SequenceStorageV1(stack));
    xl6SequenceEndV1(stack);
    if (result > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)result;
}
