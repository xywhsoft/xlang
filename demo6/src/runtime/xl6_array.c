#include "src/runtime/xl6_array.h"
#include <limits.h>

#define PARAMS uint64_t identity, uint64_t elementIdentity, unsigned int transfer, size_t size, const xrttype* type
#define LAYOUT identity,elementIdentity,transfer,size,type
static bool allocation(bool ok)
{ if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return ok; }
static bool range(int64_t value)
{
    if (value < 0 || (uint64_t)value > SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    return true;
}
xvalue* xl6ArrayCreateV1(PARAMS)
{
    if (!xl6SequenceTupleV1(LAYOUT)) return NULL;
    return xl6SequenceWrapV1(xrtTypedArrayCreate(xl6SequenceStorageTypeV1(transfer,type)),LAYOUT);
}
bool xl6ArrayPushV1(const xvalue* value, PARAMS, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayPush(xl6SequenceStorageV1(owner),item));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArraySetV1(const xvalue* value, PARAMS, int64_t index, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!range(index)) return false;
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArraySet(xl6SequenceStorageV1(owner),(size_t)index,item));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayInsertV1(const xvalue* value, PARAMS, int64_t index, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!range(index)) return false;
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayInsert(xl6SequenceStorageV1(owner),(size_t)index,item));
    xl6SequenceEndV1(owner); return ok;
}
void xl6ArrayReadV1(const xvalue* value, PARAMS, int64_t index, void* target)
{
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return; }
    if (!range(index)) return;
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return;
    xtypedarray* storage = xl6SequenceStorageV1(owner);
    const void* item = xrtTypedArrayConstGet(storage,(size_t)index);
    if (item) (void)allocation(xrtTypeCopyValue(xrtTypedArrayItemType(storage),target,item));
    xl6SequenceEndV1(owner);
}
void xl6ArrayTakeV1(const xvalue* value, PARAMS, int64_t index, void* target)
{
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return; }
    if (!range(index)) return;
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return;
    (void)allocation(xrtTypedArrayTake(xl6SequenceStorageV1(owner),(size_t)index,target));
    xl6SequenceEndV1(owner);
}
bool xl6ArrayRemoveV1(const xvalue* value, PARAMS, int64_t index, int64_t count)
{
    if (!range(index) || !range(count)) return false;
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = xrtTypedArrayRemove(xl6SequenceStorageV1(owner),(size_t)index,(size_t)count);
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayReserveV1(const xvalue* value, PARAMS, int64_t capacity)
{
    if (!range(capacity)) return false;
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayReserve(xl6SequenceStorageV1(owner),(size_t)capacity));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayResizeV1(const xvalue* value, PARAMS, int64_t count)
{
    if (!range(count)) return false;
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayResize(xl6SequenceStorageV1(owner),(size_t)count));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayTrimV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayTrim(xl6SequenceStorageV1(owner)));
    xl6SequenceEndV1(owner); return ok;
}
void xl6ArrayClearV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return;
    xerror* saved = xrtTakeError(); xrtTypedArrayClear(xl6SequenceStorageV1(owner));
    xrtClearError(); xrtSetErrorTake(saved); xl6SequenceEndV1(owner);
}
int64_t xl6ArraySizeV1(const xvalue* value, PARAMS, bool capacity)
{
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return 0;
    xtypedarray* storage = xl6SequenceStorageV1(owner);
    size_t count = capacity ? xrtTypedArrayCapacity(storage) : xrtTypedArrayCount(storage);
    xl6SequenceEndV1(owner);
    if (count > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)count;
}
xvalue* xl6ArrayCloneV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return NULL;
    xtypedarray* copy = xrtTypedArrayClone(xl6SequenceStorageV1(owner));
    xl6SequenceEndV1(owner);
    return xl6SequenceWrapV1(copy,LAYOUT);
}
bool xl6ArrayEqualsV1(const xvalue* left, const xvalue* right, PARAMS)
{ return xl6SequenceEqualsV1(left,right,LAYOUT); }
bool xl6ArrayAppendV1(const xvalue* left, const xvalue* right, PARAMS)
{
    XL6_Sequence* a = xl6SequenceBeginV1(left,LAYOUT);
    if (!a) return false;
    XL6_Sequence* b = xl6SequenceBeginOtherV1(right,LAYOUT,a);
    if (!b) { xl6SequenceEndV1(a); return false; }
    bool ok = allocation(xrtTypedArrayAppend(xl6SequenceStorageV1(a),xl6SequenceStorageV1(b)));
    if (b != a) xl6SequenceEndV1(b);
    xl6SequenceEndV1(a); return ok;
}
bool xl6ArraySwapV1(const xvalue* value, PARAMS, int64_t left, int64_t right)
{
    if (!range(left) || !range(right)) return false;
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArraySwap(xl6SequenceStorageV1(owner),(size_t)left,(size_t)right));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayReverseV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayReverse(xl6SequenceStorageV1(owner)));
    xl6SequenceEndV1(owner); return ok;
}
int64_t xl6ArrayFindV1(const xvalue* value, PARAMS, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    XL6_Sequence* owner = xl6SequenceBeginV1(value,LAYOUT);
    if (!owner) return -1;
    xtypedarray* storage = xl6SequenceStorageV1(owner);
    size_t found = SIZE_MAX;
    if (transfer != 3) found = xrtTypedArrayFind(storage,item);
    else {
        /* Managed references have equality, but no invented Compare op. */
        const xvalue* search = *(xvalue* const*)item;
        size_t count = xrtTypedArrayCount(storage);
        for (size_t i = 0; i < count; ++i) {
            const xvalue* current = *(xvalue* const*)xrtTypedArrayConstGet(storage,i);
            if (current == search || (current && search && xrtValueEqual(current,search))) {
                found = i; break;
            }
            if (xrtGetError()) break;
        }
    }
    xl6SequenceEndV1(owner);
    if (found == SIZE_MAX) return -1;
    if (found > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return -1; }
    return (int64_t)found;
}
#undef LAYOUT
#undef PARAMS
