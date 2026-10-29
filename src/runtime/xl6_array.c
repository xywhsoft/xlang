#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_index.h"
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
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    bool ok = xl6SequenceAdmitSlotV1(owner,item) && allocation(xrtTypedArrayPush(xl6SequenceStorageV1(owner),item));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArraySetV1(const xvalue* value, PARAMS, int64_t index, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    xtypedarray* storage=xl6SequenceStorageV1(owner); size_t at;
    bool ok = xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),false,&at) &&
        xl6SequenceAdmitSlotV1(owner,item) && allocation(xrtTypedArraySet(storage,at,item));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayInsertV1(const xvalue* value, PARAMS, int64_t index, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    xtypedarray* storage=xl6SequenceStorageV1(owner); size_t at;
    bool ok = xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),true,&at) &&
        xl6SequenceAdmitSlotV1(owner,item) && allocation(xrtTypedArrayInsert(storage,at,item));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayReadV1(const xvalue* value, PARAMS, int64_t index, void* target)
{
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    xtypedarray* storage = xl6SequenceStorageV1(owner);
    size_t at;
    const void* item = xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),false,&at) ?
        xrtTypedArrayConstGet(storage,at) : NULL;
    XL6_SequenceAdmissionV3 policy={0}; bool ok=false;
    if (item && transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1) {
        if (xl6SequenceAdmissionCopyV3(owner,&policy)) {
            const XL6_SequenceNativeValuePolicyV1* record=xl6SequenceNativeValuePolicyV1(&policy);
            if (record) ok=record->Copy(item,target);
            else xrtSetErrorKind(XERR_STATE);
        }
        xl6SequenceAdmissionDropV3(&policy);
    } else ok=item && allocation(xrtTypeCopyValue(xrtTypedArrayItemType(storage),target,item));
    xl6SequenceEndV1(owner);
    return ok;
}
bool xl6ArrayTakeV1(const xvalue* value, PARAMS, int64_t index, void* target)
{
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    xtypedarray* storage=xl6SequenceStorageV1(owner); size_t at;
    bool ok = xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),false,&at) &&
        allocation(xrtTypedArrayTake(storage,at,target));
    xl6SequenceEndV1(owner);
    return ok;
}
bool xl6ArrayRemoveV1(const xvalue* value, PARAMS, int64_t index, int64_t count)
{
    if (!range(count)) return false;
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    xtypedarray* storage=xl6SequenceStorageV1(owner); size_t at;
    bool ok = xl6ArrayIndexV1(index,xrtTypedArrayCount(storage),true,&at) &&
        xrtTypedArrayRemove(storage,at,(size_t)count);
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayReserveV1(const xvalue* value, PARAMS, int64_t capacity)
{
    if (!range(capacity)) return false;
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayReserve(xl6SequenceStorageV1(owner),(size_t)capacity));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayResizeV1(const xvalue* value, PARAMS, int64_t count)
{
    if (!range(count)) return false;
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xl6SequenceResizeStorageV1(owner,(size_t)count));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayTrimV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayTrim(xl6SequenceStorageV1(owner)));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayClearV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    xrtTypedArrayClear(xl6SequenceStorageV1(owner));
    xl6SequenceEndV1(owner);
    return !xrtGetError();
}
int64_t xl6ArraySizeV1(const xvalue* value, PARAMS, bool capacity)
{
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return 0;
    xtypedarray* storage = xl6SequenceStorageV1(owner);
    size_t count = capacity ? xrtTypedArrayCapacity(storage) : xrtTypedArrayCount(storage);
    xl6SequenceEndV1(owner);
    if (count > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)count;
}
xvalue* xl6ArrayCloneV1(const xvalue* value, PARAMS)
{ return xl6SequenceCloneRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT); }
bool xl6ArrayEqualsV1(const xvalue* left, const xvalue* right, PARAMS)
{ return xl6SequenceEqualsV1(left,right,LAYOUT); }
bool xl6ArrayAppendV1(const xvalue* left, const xvalue* right, PARAMS)
{
    XL6_Sequence* a = xl6SequenceBeginRoleV1(left,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!a) return false;
    XL6_Sequence* b = xl6SequenceBeginOtherV1(right,LAYOUT,a);
    if (!b) { xl6SequenceEndV1(a); return false; }
    bool ok = xl6SequenceAdmissionEquivalentV1(a,b) &&
        allocation(xrtTypedArrayAppend(xl6SequenceStorageV1(a),xl6SequenceStorageV1(b)));
    if (b != a) xl6SequenceEndV1(b);
    xl6SequenceEndV1(a); return ok;
}
bool xl6ArraySwapV1(const xvalue* value, PARAMS, int64_t left, int64_t right)
{
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    xtypedarray* storage=xl6SequenceStorageV1(owner); size_t count=xrtTypedArrayCount(storage),a,b;
    bool ok = xl6ArrayIndexV1(left,count,false,&a) && xl6ArrayIndexV1(right,count,false,&b) &&
        allocation(xrtTypedArraySwap(storage,a,b));
    xl6SequenceEndV1(owner); return ok;
}
bool xl6ArrayReverseV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return false;
    bool ok = allocation(xrtTypedArrayReverse(xl6SequenceStorageV1(owner)));
    xl6SequenceEndV1(owner); return ok;
}
int64_t xl6ArrayFindV1(const xvalue* value, PARAMS, const void* item)
{
    if (!item) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    XL6_Sequence* owner = xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return -1;
    xtypedarray* storage = xl6SequenceStorageV1(owner);
    size_t found = SIZE_MAX;
    if (transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1) {
        XL6_SequenceAdmissionV3 policy={0};
        if (xl6SequenceAdmissionCopyV3(owner,&policy)) {
            const XL6_SequenceNativeValuePolicyV1* record=xl6SequenceNativeValuePolicyV1(&policy);
            const xtypedsetkeypolicy* content=record ? record->content : NULL;
            if (!content) xrtSetErrorKind(XERR_STATE);
            size_t count=xrtTypedArrayCount(storage);
            for (size_t i=0;content && i<count;++i) {
                const void* current=xrtTypedArrayConstGet(storage,i);
                if (content->Equal(current,item,content->UserData)) { found=i; break; }
                if (xrtGetError()) break;
            }
        }
        xl6SequenceAdmissionDropV3(&policy);
    }
    else if (transfer != 3) found = xrtTypedArrayFind(storage,item);
    else {
        /* Managed references have equality, but no invented Compare op. */
        const xvalue* search = *(xvalue* const*)item;
        size_t count = xrtTypedArrayCount(storage);
        for (size_t i = 0; i < count; ++i) {
            const xvalue* current = *(xvalue* const*)xrtTypedArrayConstGet(storage,i);
            if (xl6SequenceReferenceEqualV1(current,search)) {
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
