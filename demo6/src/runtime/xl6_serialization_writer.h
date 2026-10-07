#ifndef XL6_SERIALIZATION_WRITER_H
#define XL6_SERIALIZATION_WRITER_H
#include "src/runtime/xl6_text_value.h"

/* Stateful Writer failures and allocation failures are different outcomes.
 * The bool out parameter is the language value; status never depends on
 * allocating an Error object. Preserve the caller's prior error on success. */
static inline bool xl6SerializeWriterFinishV1(bool written, xerror* prior, bool* result)
{
    if (xrtErrorKind(xrtGetError()) == XERR_MEMORY) {
        xerror* failure = xrtTakeError();
        xrtErrorFree(prior);
        xrtSetErrorTake(failure);
        return false;
    }
    xrtClearError(); xrtSetErrorTake(prior);
    *result = written;
    return true;
}

/* Shared primitive-method physical ABI. Parenthesized signatures/expressions
 * retain exact declared scalar widths and byte views without boxing. */
#define XL6_SERIALIZATION_WRITER_STEP_V1(Name, Signature, Expression) \
static inline xlir_result_bool Name Signature \
{ \
    xerror* prior = xrtTakeError(); bool written = Expression; \
    bool result = false; bool status = xl6SerializeWriterFinishV1(written, prior, &result); \
    return (xlir_result_bool){status, result}; \
}

#define XL6_SERIALIZATION_WRITER_VALUE_V1(Name, Type, Value) \
static inline bool Name(const xvalue* wire, void* writer, bool* result) \
{ \
    xerror* prior; bool written; \
    if (result == NULL || wire == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; } \
    *result = false; prior = xrtTakeError(); \
    written = writer != NULL && Value((Type*)writer, wire); \
    return xl6SerializeWriterFinishV1(written, prior, result); \
}
XL6_SERIALIZATION_WRITER_VALUE_V1(xl6JsonWriterValueV1, xjsonwriter, xrtJsonWriterValue)
XL6_SERIALIZATION_WRITER_VALUE_V1(xl6XlonWriterValueV1, xxlonwriter, xrtXlonWriterValue)
#undef XL6_SERIALIZATION_WRITER_VALUE_V1

#define XL6_SERIALIZATION_WRITER_NAME_V1(Name, Type, Key, Value) \
static inline bool Name(const xvalue* wire, void* writer, xlir_string key, bool* result) \
{ \
    xerror* prior; bool written; \
    if (result == NULL || wire == NULL || (key.Size != 0 && key.Data == NULL)) { \
        xrtSetErrorKind(XERR_ARGUMENT); return false; \
    } \
    *result = false; prior = xrtTakeError(); \
    written = writer != NULL && Key((Type*)writer, key) && Value((Type*)writer, wire); \
    return xl6SerializeWriterFinishV1(written, prior, result); \
}
XL6_SERIALIZATION_WRITER_NAME_V1(xl6JsonWriterNameV1, xjsonwriter, xrtJsonWriterName, xrtJsonWriterValue)
XL6_SERIALIZATION_WRITER_NAME_V1(xl6XlonWriterNameV1, xxlonwriter, xrtXlonWriterName, xrtXlonWriterValue)
#undef XL6_SERIALIZATION_WRITER_NAME_V1

static inline bool xl6XlonWriterIndexV1(const xvalue* wire, void* writer, int64_t index, bool* result)
{
    xerror* prior; bool written;
    if (result == NULL || wire == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    *result = false; prior = xrtTakeError();
    written = writer != NULL && xrtXlonWriterKey((xxlonwriter*)writer, index) &&
        xrtXlonWriterValue((xxlonwriter*)writer, wire);
    return xl6SerializeWriterFinishV1(written, prior, result);
}
#endif
