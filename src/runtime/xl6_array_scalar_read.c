#include "src/runtime/xl6_array_scalar_read.h"
#include "src/runtime/xl6_array_index.h"
#include <string.h>

/* -1 failure, 0 a borrowed wire item, 1 a native item with its pin still held.
 * Native pointer validation precedes every dereference. The selected built-in
 * descriptor is exact, not a same-sized custom descriptor or a converter. */
static int begin(const xvalue* source, uint64_t identity, uint64_t elementIdentity,
    int64_t index, const xrttype* type, size_t size, XL6_Sequence** pin,
    const void** slot, const xvalue** wire)
{
    if (!source || !identity || !elementIdentity) {
        xrtSetErrorKind(XERR_ARGUMENT); return -1;
    }
    XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter = NULL;
    int status = xl6SequenceBeginErasedArrayV1(source, pin, &layout, &converter);
    if (status < 0) return -1;
    if (status) {
        if (layout.identity != identity || layout.elementIdentity != elementIdentity ||
            layout.transfer != 1 ||
            layout.type != type || layout.size != size) {
            xl6SequenceEndV1(*pin); *pin = NULL;
            xrtSetErrorKind(XERR_TYPE); return -1;
        }
        xtypedarray* storage = xl6SequenceStorageV1(*pin);
        size_t at;
        *slot = xl6ArrayIndexV1(index, xrtTypedArrayCount(storage), false, &at)
            ? xrtTypedArrayConstGet(storage, at) : NULL;
        if (!*slot) {
            xl6SequenceEndV1(*pin); *pin = NULL;
            if (!xrtGetError()) xrtSetErrorKind(XERR_STATE);
            return -1;
        }
        return 1;
    }
    if (xrtValueType(source) != XVALUE_ARRAY) {
        xrtSetErrorKind(XERR_TYPE); return -1;
    }
    *wire = xrtValueArrayAt(source, index);
    if (!*wire) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_RANGE);
        return -1;
    }
    return 0;
}

#define READ(Name, CType, Type, Get) \
CType Name(const xvalue* source, uint64_t identity, uint64_t elementIdentity, int64_t index) \
{ \
    CType result = 0; XL6_Sequence* pin = NULL; \
    const void* slot = NULL; const xvalue* wire = NULL; \
    int status = begin(source, identity, elementIdentity, index, Type(), sizeof(result), &pin, &slot, &wire); \
    if (status == 1) { memcpy(&result, slot, sizeof(result)); xl6SequenceEndV1(pin); } \
    else if (status == 0 && !Get(wire, &result) && !xrtGetError()) xrtSetErrorKind(XERR_TYPE); \
    return result; \
}
READ(xl6ArrayReadI64V1, int64_t, xrtTypeInt64, xrtValueGetInt)
READ(xl6ArrayReadU64V1, uint64_t, xrtTypeUInt64, xrtValueGetUInt)
READ(xl6ArrayReadBoolV1, bool, xrtTypeBool, xrtValueGetBool)
READ(xl6ArrayReadF64V1, double, xrtTypeFloat64, xrtValueGetFloat)
#undef READ
