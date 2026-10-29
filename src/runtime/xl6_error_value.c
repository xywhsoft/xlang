#include "src/runtime/xl6_error_value.h"

static bool errorClone(void* payload, void** output, void* user)
{
    (void)user;
    if (!payload || !output) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    *output = xrtErrorRef(payload);
    return *output != NULL;
}
static void errorDrop(void* payload, void* user)
{ (void)user; xrtErrorFree(payload); }
static const xvaluehandleops errorOps = {errorClone, errorDrop, NULL, NULL};
static bool errorTrace(const xvalue*, xrtownershipvisitor, void*);

const xerror* xl6ErrorValueBorrowV1(const xvalue* value)
{
    void* payload = NULL;
    void* user = NULL;
    const xvaluehandleops* ops = NULL;
    if (!value || xrtValueType(value) != XVALUE_HANDLE ||
        !xrtValueHandleOwnershipAdapterV1(xrtValueOwnership(value), &errorOps, errorTrace))
        return NULL;
    if (!xrtValueGetHandle(value, &payload, &ops, &user) ||
        ops != &errorOps || user || !xrtValueTypeId(value)) return NULL;
    return payload;
}
static bool errorTrace(const xvalue* value, xrtownershipvisitor visit, void* context)
{
    const xerror* payload = xl6ErrorValueBorrowV1(value);
    if (!payload || !visit) { xrtSetErrorKind(XERR_TYPE); return false; }
    xrtownershipref child = xrtErrorOwnership(payload);
    return !child.Data || visit(child, context);
}
const xrtownershipadapterv1* xl6ErrorValueOwnershipAdapterV1(xrtownershipref ref)
{ return xrtValueHandleOwnershipAdapterV1(ref, &errorOps, errorTrace); }

xvalue* xl6ErrorValueRefV1(const xerror* error, uint64_t identity)
{
    if (!identity) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!error) return xrtValueRetain(xrtValueNull());
    void* owned = xrtErrorRef(error);
    if (!owned) return NULL;
    xvalue* value = xrtValueHandleTake(&owned, &errorOps, NULL);
    if (value && xrtValueTypeIdBind(value, identity) &&
        xrtValueHandleOwnershipBindPhased(value, errorTrace)) return value;
    xerror* failure = xrtTakeError();
    if (value) xrtValueRelease(value);
    else if (owned) xrtErrorFree(owned);
    xrtClearError();
    xrtSetErrorTake(failure);
    return NULL;
}
bool xl6ErrorValueReadRefV1(const xvalue* value, uint64_t identity, xerror** output)
{
    if (!value || !identity || !output) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (xrtValueType(value) == XVALUE_NULL) { *output = NULL; return true; }
    const xerror* borrowed = xl6ErrorValueBorrowV1(value);
    if (!borrowed || xrtValueTypeId(value) != identity) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    xerror* reference = xrtErrorRef(borrowed);
    if (!reference) return false;
    *output = reference;
    return true;
}
