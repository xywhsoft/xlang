/* Standalone C tests/examples have only XRT wire values, not native xlang
 * handles. This provider preserves that boundary without linking a second
 * XRT or the language runtime. Production language packages use the resident
 * xl_value_read.h implementation. Define IMPLEMENTATION in exactly one TU. */
#ifndef XL_VALUE_READ_WIRE_H
#define XL_VALUE_READ_WIRE_H
#define xlValueBorrowV1 xlWireValueBorrowV1
#define xlValueReadBeginV1 xlWireValueReadBeginV1
#define xlValueReadEndV1 xlWireValueReadEndV1
#define xlValueReadAtV1 xlWireValueReadAtV1
#define xlValueReadFindV1 xlWireValueReadFindV1
#define xlValueBorrowCopyV1 xlWireValueBorrowCopyV1
#include "../../include/xl_value_read.h"
#endif

#if defined(XL_VALUE_READ_WIRE_IMPLEMENTATION) && !defined(XL_VALUE_READ_WIRE_IMPLEMENTED)
#define XL_VALUE_READ_WIRE_IMPLEMENTED
XL_ValueBorrowV1 xlWireValueBorrowV1(const xvalue* source)
{
    XL_ValueBorrowV1 value = {0};
    if (!source) return value;
    value.ref = source; value.kind = xrtValueType(source);
    switch (value.kind) {
    case XVALUE_BOOL: (void)xrtValueGetBool(source, &value.data.boolean); break;
    case XVALUE_INT: (void)xrtValueGetInt(source, &value.data.integer); break;
    case XVALUE_UINT: (void)xrtValueGetUInt(source, &value.data.unsignedInteger); break;
    case XVALUE_FLOAT: (void)xrtValueGetFloat(source, &value.data.number); break;
    case XVALUE_STRING: (void)xrtValueGetString(source, &value.data.text); break;
    case XVALUE_BYTES: (void)xrtValueGetBytes(source, &value.data.bytes); break;
    case XVALUE_POINTER: { ptr raw = NULL; (void)xrtValueGetPointer(source, &raw);
        value.data.unsignedInteger = (uintptr_t)raw; break; }
    default: break;
    }
    return value;
}
bool xlWireValueReadBeginV1(XL_ValueBorrowV1 value, XL_ValueReadV1* view)
{
    if (!view || view->self || view->source || view->mode) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    if (!xrtValueViewBeginV1(value.ref, &view->wire)) return false;
    view->self = view; view->source = value.ref; view->mode = 1;
    view->kind = view->wire.Type; view->count = view->wire.Count; return true;
}
bool xlWireValueReadEndV1(XL_ValueReadV1* view)
{
    if (view && !view->self && !view->source && !view->mode) return true;
    if (!view || view->self != view) { xrtSetErrorKind(XERR_STATE); return false; }
    if (!xrtValueViewEndV1(&view->wire)) return false;
    *view = (XL_ValueReadV1){0}; return true;
}
XL_ValueBorrowV1 xlWireValueReadAtV1(const XL_ValueReadV1* view, size_t index, xstrview* key)
{ return xlWireValueBorrowV1(xrtValueViewAtV1(view ? &view->wire : NULL, index, key)); }
XL_ValueBorrowV1 xlWireValueReadFindV1(const XL_ValueReadV1* view, xstrview key)
{ return xlWireValueBorrowV1(xrtValueViewFindV1(view ? &view->wire : NULL, key)); }
xvalue* xlWireValueBorrowCopyV1(XL_ValueBorrowV1 value)
{
    if (!value.ref && value.kind != XVALUE_NULL) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xrtValueRetain(value.ref ? value.ref : xrtValueNull());
}
#endif
