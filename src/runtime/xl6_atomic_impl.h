/* Private typed instantiation, one resident source/ownership domain per family.
 * Only scalar state is stored: no language graph edges, callbacks or borrowed
 * memory. All state operations use XRT atomics, never a second atomic engine. */
#include "xl6_atomic_internal.h"

#define XL6_ATOMIC_NAME_(Name, Suffix) xl6Atomic##Name##Suffix##V1
#define XL6_ATOMIC_NAME(Name, Suffix) XL6_ATOMIC_NAME_(Name, Suffix)
#define XL6_ATOMIC_FN(Suffix) XL6_ATOMIC_NAME(XL6_ATOMIC_FAMILY, Suffix)
#define XL6_ATOMIC_NATIVE_(Width, Suffix) xrtAtomic##Width##Suffix
#define XL6_ATOMIC_NATIVE_EXPAND(Width, Suffix) XL6_ATOMIC_NATIVE_(Width, Suffix)
#define XL6_ATOMIC_NATIVE(Suffix) XL6_ATOMIC_NATIVE_EXPAND(XL6_ATOMIC_WIDTH, Suffix)

typedef struct XL6_AtomicState { XL6_ATOMIC_STORAGE value; } XL6_AtomicState;

static XL6_AtomicState* xl6AtomicState(void* raw)
{
    if (!raw) xrtSetErrorKind(XERR_STATE);
    return raw;
}

void* XL6_ATOMIC_FN(Create)(XL6_ATOMIC_VALUE initial)
{
    if (!XL6_ATOMIC_VALIDATE(initial)) return NULL;
    XL6_AtomicState* state = xrtMalloc(sizeof(*state));
    if (state) XL6_ATOMIC_NATIVE(Init)(&state->value, (XL6_ATOMIC_BITS)initial);
    return state;
}

void XL6_ATOMIC_FN(Destroy)(void* raw) { xrtFree(raw); }

XL6_ATOMIC_VALUE XL6_ATOMIC_FN(Load)(void* raw, int64_t order)
{
    XL6_AtomicState* state = xl6AtomicState(raw);
    if (!state || !xl6AtomicOrder(order, true, false)) return 0;
    return XL6_ATOMIC_DECODE(XL6_ATOMIC_NATIVE(Load)(&state->value, (xmemoryorder)order));
}

void XL6_ATOMIC_FN(Store)(void* raw, XL6_ATOMIC_VALUE value, int64_t order)
{
    XL6_AtomicState* state = xl6AtomicState(raw);
    if (!state || !xl6AtomicOrder(order, false, true) || !XL6_ATOMIC_VALIDATE(value)) return;
    XL6_ATOMIC_NATIVE(Store)(&state->value, (XL6_ATOMIC_BITS)value, (xmemoryorder)order);
}

XL6_ATOMIC_VALUE XL6_ATOMIC_FN(Exchange)(void* raw, XL6_ATOMIC_VALUE value, int64_t order)
{
    XL6_AtomicState* state = xl6AtomicState(raw);
    if (!state || !xl6AtomicOrder(order, false, false) || !XL6_ATOMIC_VALIDATE(value)) return 0;
    return XL6_ATOMIC_DECODE(XL6_ATOMIC_NATIVE(Exchange)(&state->value, (XL6_ATOMIC_BITS)value, (xmemoryorder)order));
}

XL6_ATOMIC_PRODUCT XL6_ATOMIC_FN(CompareExchange)(void* raw, XL6_ATOMIC_VALUE expected,
    XL6_ATOMIC_VALUE desired, int64_t successOrder, int64_t failureOrder)
{
    XL6_AtomicState* state = xl6AtomicState(raw);
    if (!state || !xl6AtomicCompareOrders(successOrder, failureOrder) ||
        !XL6_ATOMIC_VALIDATE(expected) || !XL6_ATOMIC_VALIDATE(desired))
        return (XL6_ATOMIC_PRODUCT){false, 0};
    XL6_ATOMIC_BITS observed = (XL6_ATOMIC_BITS)expected;
    bool exchanged = XL6_ATOMIC_NATIVE(CompareExchange)(&state->value, &observed,
        (XL6_ATOMIC_BITS)desired, (xmemoryorder)successOrder, (xmemoryorder)failureOrder);
    return (XL6_ATOMIC_PRODUCT){exchanged, XL6_ATOMIC_DECODE(observed)};
}

#if XL6_ATOMIC_INTEGER
#define XL6_ATOMIC_FETCH(Operation) \
XL6_ATOMIC_VALUE XL6_ATOMIC_FN(Fetch##Operation)(void* raw, XL6_ATOMIC_VALUE value, int64_t order) \
{ \
    XL6_AtomicState* state = xl6AtomicState(raw); \
    if (!state || !xl6AtomicOrder(order, false, false) || !XL6_ATOMIC_VALIDATE(value)) return 0; \
    return XL6_ATOMIC_DECODE(XL6_ATOMIC_NATIVE(Fetch##Operation)(&state->value, \
        (XL6_ATOMIC_BITS)value, (xmemoryorder)order)); \
}
XL6_ATOMIC_FETCH(Add)
XL6_ATOMIC_FETCH(Sub)
XL6_ATOMIC_FETCH(And)
XL6_ATOMIC_FETCH(Or)
XL6_ATOMIC_FETCH(Xor)
#undef XL6_ATOMIC_FETCH
#endif

XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(XL6_ATOMIC_POLICY, XL6_ATOMIC_FN(Destroy))
