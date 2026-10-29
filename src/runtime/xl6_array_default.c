#include "src/runtime/xl6_array_default.h"
#include "src/runtime/xl6_array_method_view.h"

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
    if (!array || abi<=XL6_VALUE_DEFAULT_INVALID || abi>XL6_VALUE_DEFAULT_TIME) {
        xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    if (target<0 || (uint64_t)target>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    ArrayView view = {0};
    if (!begin(array,&view)) return false;
    if (view.owner) {
        /* The sealed resident policy, not the boxed compatibility hint, owns
         * defaultability and the actual slot initializer. Keep the receiver
         * pinned through initialization, rollback and committed destruction. */
        xerror* prior=xrtTakeError();
        bool ok=allocated(xl6SequenceResizeStorageV1(view.owner,(size_t)target));
        xerror* failure=xrtTakeError();
        ok=ok && failure==NULL;
        if (prior) { xrtErrorFree(failure); xrtSetErrorTake(prior); }
        else xrtSetErrorTake(failure);
        finish(&view);
        return ok;
    }
    size_t original = count(&view), size = (size_t)target;
    bool ok = false;
    if (size<original) {
        ok=xrtValueArrayRemove(view.wire,size,original-size);
        finish(&view); return ok;
    }
    if (size==original) { finish(&view); return true; }
    if (abi==XL6_VALUE_DEFAULT_FORBIDDEN) {
        xrtSetErrorKind(XERR_TYPE);
        finish(&view);
        return false;
    }
    /* Never reserve/mutate the receiver before the full delta is ready. XRT
     * owns copying, allocation, backing snapshots and atomic graph commit. */
    xvalue* item = valueDefault(abi), *delta = NULL;
    if (!item || !(delta=xrtValueArray()) || !xrtValueReserve(delta,size-original)) goto done;
    for (size_t i=original;i<size;++i) if (!xrtValueArrayAppend(delta,item)) goto done;
    ok = xrtValueArrayExtend(view.wire, delta);
done:
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    cleanup(delta);
    cleanup(item);
    finish(&view);
    return ok;
}
