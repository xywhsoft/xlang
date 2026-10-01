#ifndef XL6_NUMBER_INTERNAL_H
#define XL6_NUMBER_INTERNAL_H
#include "src/runtime/xl6_text_internal.h"
#include <string.h>

static inline void xl6NumberError(xerrkind kind, int32_t code, const char* operation, const char* message)
{
    xerrordesc description = {0};
    description.Kind = kind; description.Code = code;
    description.Domain = "xrt.number"; description.Operation = operation; description.Message = message;
    xerror* error = xrtErrorBuild(&description);
    if (error) xrtSetErrorTake(error);
    /* Error construction may fail: never replace its MemoryError by a
     * fabricated syntax/range result. tryParse must propagate resource failure. */
}
static inline bool xl6NumberU32(int64_t value, uint32_t* output, const char* operation)
{
    if (value < 0 || (uint64_t)value > UINT32_MAX) {
        xl6NumberError(XERR_VALUE, XNUMBER_ERROR_CONFIG, operation, "numeric configuration is outside uint32");
        return false;
    }
    *output = (uint32_t)value; return true;
}
/* Writers finish in a bounded stack buffer before either allocating an owned
 * string or appending to Buffer. No query/reparse and no temporary xvalue. */
static inline bool xl6NumberWriteInt(int64_t value, int64_t base, int64_t flags,
    char* data, size_t capacity, size_t* written)
{
    uint32_t radix, options;
    return xl6NumberU32(base, &radix, "int-write") && xl6NumberU32(flags, &options, "int-write") &&
        xrtIntWrite(value, radix, data, capacity, written, options);
}
static inline bool xl6NumberWriteUInt(uint64_t value, int64_t base, int64_t flags,
    char* data, size_t capacity, size_t* written)
{
    uint32_t radix, options;
    return xl6NumberU32(base, &radix, "uint-write") && xl6NumberU32(flags, &options, "uint-write") &&
        xrtUIntWrite(value, radix, data, capacity, written, options);
}
static inline bool xl6NumberWriteFloat(double value, int64_t flags,
    char* data, size_t capacity, size_t* written)
{
    uint32_t options;
    return xl6NumberU32(flags, &options, "num-write") &&
        xrtNumWrite(value, data, capacity, written, options);
}
static inline xstrview xl6NumberTextCopy(const char* data, size_t written)
{
    char* output = xrtMalloc(written + 1);
    if (output) memcpy(output, data, written + 1); /* Writer includes a terminator. */
    return xl6TextOwn(output, written);
}
/* XRT reports numeric syntax as protocol; the language contract is ValueError.
 * Take the old immutable error before copying its diagnostic fields: borrowed
 * strings must stay alive even if replacement allocation installs MemoryError. */
static inline void xl6NumberParseError(void)
{
    const xerror* borrowed = xrtGetError();
    const char* domain = xrtErrorDomain(borrowed);
    if (xrtErrorKind(borrowed) != XERR_PROTOCOL || !domain || strcmp(domain, "xrt.number") ||
        xrtErrorCode(borrowed) != XNUMBER_ERROR_FORMAT) return;
    xerror* original = xrtTakeError();
    xerrordesc description = {0};
    description.Kind = XERR_VALUE; description.Code = xrtErrorCode(original);
    description.SystemCode = xrtErrorSystemCode(original); description.Domain = xrtErrorDomain(original);
    description.Operation = xrtErrorOperation(original); description.Message = xrtErrorMessage(original);
    description.Data = xrtErrorData(original); description.Cause = xrtErrorCause(original);
    xerrorlocation location = {xrtErrorFile(original), xrtErrorLine(original), xrtErrorColumn(original)};
    xerror* replacement = xrtErrorBuildAt(&description, &location);
    xrtErrorFree(original);
    if (replacement) xrtSetErrorTake(replacement);
}
#endif
