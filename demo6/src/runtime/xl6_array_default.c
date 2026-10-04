#include "src/runtime/xl6_array_default.h"

static xvalue* valueDefault(unsigned int abi)
{
    switch (abi) {
    case XL6_VALUE_DEFAULT_NULL: return xrtValueNull();
    case XL6_VALUE_DEFAULT_BOOL: return xrtValueBool(false);
    case XL6_VALUE_DEFAULT_INT: return xrtValueInt(0);
    case XL6_VALUE_DEFAULT_UINT: return xrtValueUInt(0);
    case XL6_VALUE_DEFAULT_FLOAT: return xrtValueFloat(0.0);
    case XL6_VALUE_DEFAULT_CHAR: return xrtValueChar(0);
    case XL6_VALUE_DEFAULT_STRING: return xrtValueString((xstrview){"", 0});
    case XL6_VALUE_DEFAULT_BYTES: return xrtValueBytes((xbytesview){NULL, 0});
    case XL6_VALUE_DEFAULT_TIME: return xrtValueTime(0);
    default: return NULL;
    }
}
static void cleanup(xvalue* value)
{
    xerror* saved = xrtTakeError();
    xrtValueRelease(value);
    xrtClearError();
    xrtSetErrorTake(saved);
}
bool xl6ArrayResizeDefaultV1(xvalue* array, int64_t target, unsigned int abi)
{
    if (!array || xrtValueType(array)!=XVALUE_ARRAY ||
        abi<=XL6_VALUE_DEFAULT_INVALID || abi>XL6_VALUE_DEFAULT_TIME) {
        xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    if (target<0 || (uint64_t)target>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    size_t count = xrtValueCount(array), size = (size_t)target;
    if (size<count) return xrtValueArrayRemove(array, size, count-size);
    if (size==count) return true;
    if (abi==XL6_VALUE_DEFAULT_FORBIDDEN) {
        xrtSetErrorKind(XERR_TYPE);
        return false;
    }
    /* Never reserve/mutate the receiver before the full delta is ready. XRT
     * owns copying, allocation, backing snapshots and atomic graph commit. */
    xvalue* item = valueDefault(abi), *delta = NULL;
    bool ok = false;
    if (!item || !(delta=xrtValueArray()) || !xrtValueReserve(delta,size-count)) goto done;
    for (size_t i=count;i<size;++i) if (!xrtValueArrayAppend(delta,item)) goto done;
    ok = xrtValueArrayExtend(array, delta);
done:
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    cleanup(delta);
    cleanup(item);
    return ok;
}
