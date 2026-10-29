#include "src/runtime/xl6_array_method_view.h"

bool xl6ArrayMethodClearV1(xvalue* source)
{
    ArrayView view = {0};
    if (!begin(source, &view)) return false;
    bool ok;
    if (view.owner) {
        xerror* prior = xrtTakeError();
        xrtTypedArrayClear(view.storage);
        xerror* cleanup = xrtTakeError();
        ok = cleanup == NULL;
        if (prior) { xrtErrorFree(cleanup); xrtSetErrorTake(prior); }
        else xrtSetErrorTake(cleanup);
    } else ok = xrtValueClear(view.wire);
    finish(&view); return ok;
}

int64_t xl6ArrayMethodCapacityV1(const xvalue* source)
{
    ArrayView view = {0};
    if (!begin(source, &view)) return 0;
    size_t size = view.owner ? xrtTypedArrayCapacity(view.storage) : xrtValueCapacity(view.wire);
    finish(&view);
    if (size > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)size;
}

bool xl6ArrayMethodReserveV1(xvalue* source, int64_t capacity)
{
    ArrayView view = {0};
    if (!extent(capacity) || !begin(source, &view)) return false;
    bool ok = allocated(view.owner ? xrtTypedArrayReserve(view.storage, (size_t)capacity)
        : xrtValueReserve(view.wire, (size_t)capacity));
    finish(&view); return ok;
}

bool xl6ArrayMethodTrimV1(xvalue* source)
{
    ArrayView view = {0};
    if (!begin(source, &view)) return false;
    bool ok = allocated(view.owner ? xrtTypedArrayTrim(view.storage) : xrtValueTrim(view.wire));
    finish(&view); return ok;
}

bool xl6ArrayMethodSwapV1(xvalue* source, int64_t left, int64_t right)
{
    ArrayView view = {0};
    if (!begin(source, &view)) return false;
    bool ok = arrayIndex(&view, left, false) && arrayIndex(&view, right, false) &&
        allocated(view.owner ? xrtTypedArraySwap(view.storage, (size_t)left, (size_t)right)
            : xrtValueArraySwap(view.wire, (size_t)left, (size_t)right));
    finish(&view); return ok;
}

bool xl6ArrayMethodReverseV1(xvalue* source)
{
    ArrayView view = {0};
    if (!begin(source, &view)) return false;
    bool ok = true;
    if (view.owner) ok = allocated(xrtTypedArrayReverse(view.storage));
    else {
        size_t size = count(&view);
        for (size_t at = 0; at < size / 2; ++at)
            if (!(ok = allocated(xrtValueArraySwap(view.wire, at, size-at-1)))) break;
    }
    finish(&view); return ok;
}
