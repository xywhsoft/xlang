#ifndef XL6_SERIALIZATION_ENCODE_H
#define XL6_SERIALIZATION_ENCODE_H
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_serialization_options.h"

/* Borrowed wire/options; exact owned UTF-8 view or checked NULL. Buffer.Size
 * is authoritative, including empty output. The terminator is not a part of
 * the language value. No strlen, graph traversal or schema inference here. */
static inline xstrview xl6SerializeBufferTakeViewV1(xbuffer* buffer)
{
    xstrview result = {NULL, 0};
    if (buffer == NULL) return result;
    result.Size = xrtBufferView(buffer).Size;
    if (!xrtBufferAppendByte(buffer, 0)) {
        xerror* failure = xrtTakeError();
        xrtBufferDestroy(buffer);
        xrtClearError(); xrtSetErrorTake(failure);
        return (xstrview){NULL, 0};
    }
    result.Data = (char*)xrtBufferTake(buffer, NULL, NULL);
    xrtBufferDestroy(buffer);
    return result.Data != NULL ? result : (xstrview){NULL, 0};
}

/* Each inline encoder mentions exactly one XRT writer. Native closure
 * selection therefore need not link other formats into a JSON-only module. */
#define XL6_SERIALIZATION_ENCODER_V1(Name, Config, Options, Write) \
static inline xstrview Name(const xvalue* wire, const xvalue* options) \
{ \
    Config config; xbuffer* buffer; \
    if (wire == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return (xstrview){NULL, 0}; } \
    if (!Options(options, &config)) return (xstrview){NULL, 0}; \
    buffer = xrtBufferCreate(); \
    if (buffer == NULL) return (xstrview){NULL, 0}; \
    if (!Write(wire, &config, xl6SerializeBufferWrite, buffer)) { \
        xerror* failure = xrtTakeError(); \
        xrtBufferDestroy(buffer); xrtClearError(); xrtSetErrorTake(failure); \
        return (xstrview){NULL, 0}; \
    } \
    return xl6SerializeBufferTakeViewV1(buffer); \
}
XL6_SERIALIZATION_ENCODER_V1(xl6JsonEncodeV1, xjsonwriteconfig, xl6JsonWriteOptions, xrtJsonWrite)
XL6_SERIALIZATION_ENCODER_V1(xl6XlonEncodeV1, xxlonwriteconfig, xl6XlonWriteOptions, xrtXlonWrite)
XL6_SERIALIZATION_ENCODER_V1(xl6JsonlEncodeV1, xjsonlwriteconfig, xl6JsonlWriteOptions, xrtJsonlWrite)
XL6_SERIALIZATION_ENCODER_V1(xl6XlonlEncodeV1, xxlonlwriteconfig, xl6XlonlWriteOptions, xrtXlonlWrite)
#undef XL6_SERIALIZATION_ENCODER_V1
#endif
