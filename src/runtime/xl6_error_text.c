#include "src/runtime/xl6_error_text.h"
#include "src/runtime/xl6_text_internal.h"
#include <string.h>

static xerrkind nativeKind(const char* kind)
{
    if (!kind) return XERR_STATE;
    if (!strcmp(kind, "TypeError")) return XERR_TYPE;
    if (!strcmp(kind, "MemoryError")) return XERR_MEMORY;
    if (!strcmp(kind, "RangeError") || !strcmp(kind, "IndexError") || !strcmp(kind, "OverflowError")) return XERR_RANGE;
    if (!strcmp(kind, "FileError") || !strcmp(kind, "IOError") || !strcmp(kind, "NetworkError") || !strcmp(kind, "EncodingError")) return XERR_IO;
    if (!strcmp(kind, "TimeoutError")) return XERR_TIMEOUT;
    if (!strcmp(kind, "CancelledError")) return XERR_CANCELLED;
    if (!strcmp(kind, "NotImplementedError")) return XERR_UNSUPPORTED;
    if (!strcmp(kind, "ValueError") || !strcmp(kind, "LookupError") || !strcmp(kind, "KeyError") ||
        !strcmp(kind, "AttributeError") || !strcmp(kind, "NameError") || !strcmp(kind, "ArithmeticError") ||
        !strcmp(kind, "ZeroDivisionError")) return XERR_VALUE;
    return XERR_STATE;
}
static bool contains(xstrview text, const char* needle)
{
    size_t size = strlen(needle);
    if (size > text.Size) return false;
    for (size_t i = 0; i <= text.Size - size; ++i)
        if (!memcmp(text.Data + i, needle, size)) return true;
    return false;
}
const char* xl6ErrorKindNameV1(const xerror* error)
{
    xstrview domain = xrtErrorDomainView(error), data = xrtErrorDataView(error);
    xerrkind kind = xrtErrorKind(error);
    /* Language kinds are identifiers, never binary diagnostic payloads. Do
     * not authenticate an embedded-NUL domain or truncate a forged kind. */
    if (domain.Size == 5 && !memcmp(domain.Data, "xlang", 5) &&
        data.Size && !memchr(data.Data, 0, data.Size)) return data.Data;
    if (kind == XERR_ARGUMENT || kind == XERR_VALUE || kind == XERR_EXISTS) return "ValueError";
    if (kind == XERR_TYPE) return "TypeError";
    if (kind == XERR_RANGE) return "RangeError";
    if (kind == XERR_MEMORY) return "MemoryError";
    if (kind == XERR_TIMEOUT) return "TimeoutError";
    if (kind == XERR_CANCELLED) return "CancelledError";
    if (kind == XERR_UNSUPPORTED) return "NotImplementedError";
    if (kind == XERR_NOT_FOUND) return contains(domain, "file") ? "FileError" : "LookupError";
    if (kind == XERR_IO || kind == XERR_PERMISSION || kind == XERR_PROTOCOL) {
        if (contains(domain, "file")) return "FileError";
        if (contains(domain, "net") || contains(domain, "tls")) return "NetworkError";
        return "IOError";
    }
    return "RuntimeError";
}
static const char* parentKind(const char* kind)
{
    if (!kind || !*kind || !strcmp(kind, "Error")) return NULL;
    if (!strcmp(kind, "AssertionError") || !strcmp(kind, "NullReferenceError") || !strcmp(kind, "NotImplementedError") ||
        !strcmp(kind, "NativeError") || !strcmp(kind, "ArithmeticError")) return "RuntimeError";
    if (!strcmp(kind, "ZeroDivisionError") || !strcmp(kind, "OverflowError")) return "ArithmeticError";
    if (!strcmp(kind, "RangeError")) return "ValueError";
    if (!strcmp(kind, "IndexError") || !strcmp(kind, "KeyError") || !strcmp(kind, "AttributeError") ||
        !strcmp(kind, "NameError")) return "LookupError";
    if (!strcmp(kind, "FileError") || !strcmp(kind, "EncodingError") || !strcmp(kind, "NetworkError") ||
        !strcmp(kind, "TimeoutError") || !strcmp(kind, "CompressionError") || !strcmp(kind, "ArchiveError")) return "IOError";
    if (!strcmp(kind, "ModuleNotFoundError")) return "ImportError";
    if (!strcmp(kind, "FutureError") || !strcmp(kind, "TaskError") || !strcmp(kind, "CancelledError") ||
        !strcmp(kind, "GeneratorError")) return "AsyncError";
    if (!strcmp(kind, "SyntaxError") || !strcmp(kind, "CsvError") || !strcmp(kind, "ConfigError") ||
        !strcmp(kind, "TomlError") || !strcmp(kind, "PlistError") || !strcmp(kind, "XmlError")) return "ParseError";
    return "Error";
}
bool xl6ErrorKindIsV1(const xerror* error, const char* expected)
{
    const char* actual = xl6ErrorKindNameV1(error);
    if (!expected || !*expected) return true;
    while (actual && *actual) {
        if (!strcmp(actual, expected)) return true;
        actual = parentKind(actual);
    }
    return false;
}
xerror* xl6ErrorCreateV1(const char* kind, xstrview text, int32_t line, int32_t column)
{
    xerrordescview desc = { nativeKind(kind), 0, 0, {"xlang", 5}, {"throw", 5},
        text, xrtStrView(kind ? kind : ""), NULL };
    xerrorlocationview location = {{"", 0}, line, column};
    xerror* error = xrtErrorBuildViewAt(&desc, &location);
    if (!error) {
        error = xrtTakeError();
        if (!error) { xrtSetErrorKind(XERR_INTERNAL); error = xrtTakeError(); }
    }
    return error;
}
xstrview xl6ErrorMessageCopyV1(const xerror* error)
{ return xl6TextCopy(xrtErrorMessageView(error)); }
xstrview xl6ErrorDomainCopyV1(const xerror* error)
{ return xl6TextCopy(xrtErrorDomainView(error)); }
xstrview xl6ErrorOperationCopyV1(const xerror* error)
{ return xl6TextCopy(xrtErrorOperationView(error)); }
xstrview xl6ErrorDataCopyV1(const xerror* error)
{ return xl6TextCopy(xrtErrorDataView(error)); }
xstrview xl6ErrorFileCopyV1(const xerror* error)
{ return xl6TextCopy(xrtErrorFileView(error)); }
xstrview xl6ErrorKindCopyV1(const xerror* error)
{ return xl6TextCopy(xrtStrView(xl6ErrorKindNameV1(error))); }
void xl6ErrorWrapAsyncV1(const char* fallback)
{
    xerror* cause = xrtTakeError();
    xerrordescview desc = { XERR_STATE, 1, 0, {"xlang", 5}, {"async", 5},
        cause ? xrtErrorMessageView(cause) : xrtStrView(fallback ? fallback : ""), {"AsyncError", 10}, cause };
    xerror* wrapped = xrtErrorBuildView(&desc);
    xerror* failure = wrapped ? NULL : xrtTakeError();
    xrtErrorFree(cause);
    xrtClearError();
    if (!wrapped && !failure) { xrtSetErrorKind(XERR_INTERNAL); failure = xrtTakeError(); }
    xrtSetErrorTake(wrapped ? wrapped : failure);
}
