#include "src/runtime/xl6_regex_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Immutable XRT regex is the native field itself: no second refcount layer,
 * no exposed matcher, and no generated implementation in Cgen. */
void* xl6RegexCreateV1(xstrview pattern, int64_t flags, int64_t maxPatternBytes,
    int64_t maxCaptures, int64_t size)
{
    xregexconfig config;
    if (!xl6RegexConfig(flags,maxPatternBytes,maxCaptures,&config)) return NULL;
    if (!xl6TextSelect(&pattern, size)) return NULL;
    return xrtRegexCompileConfig(pattern, &config);
}
void xl6RegexDestroyV1(void* handle) { xrtRegexRelease(handle); }
int64_t xl6RegexErrorOffsetV1(const xerror* error)
{
    for (;error;error=xrtErrorCause(error)) {
        size_t offset;
        if (xrtRegexErrorOffset(error,&offset) && offset<=INT64_MAX) return (int64_t)offset;
    }
    return -1;
}
static bool xl6RegexCheck(void* handle)
{
    if (handle) return true;
    xrtSetErrorKind(XERR_ARGUMENT); return false;
}
xstrview xl6RegexPatternV1(void* handle)
{ return xl6RegexCheck(handle) ? xl6TextCopy(xrtRegexPattern(handle)) : xl6TextFailure; }
int64_t xl6RegexFlagsV1(void* handle)
{ return xl6RegexCheck(handle) ? (int64_t)xrtRegexFlags(handle) : 0; }
int64_t xl6RegexCaptureCountV1(void* handle)
{ return xl6RegexCheck(handle) ? (int64_t)xrtRegexCaptureCount(handle) : 0; }
xstrview xl6RegexCaptureNameV1(void* handle, int64_t index)
{
    if (!xl6RegexCheck(handle)) return xl6TextFailure;
    if (index < 0 || (uint64_t)index >= xrtRegexCaptureCount(handle)) {
        xrtSetErrorKind(XERR_RANGE); return xl6TextFailure;
    }
    xstrview name;
    return xrtRegexCaptureName(handle, (size_t)index, &name) ? xl6TextCopy(name) : xl6TextFailure;
}
int64_t xl6RegexCaptureIndexV1(void* handle, xstrview name, int64_t size)
{
    if (!xl6RegexCheck(handle) || !xl6TextSelect(&name, size)) return -1;
    /* An empty name is not the name of every unnamed group. */
    if (!name.Size) return -1;
    size_t index = xrtRegexCaptureIndex(handle, name);
    return index == XRT_NPOS ? -1 : (int64_t)index;
}
xstrview xl6RegexEscapeV1(xstrview text, int64_t size)
{
    if (!xl6TextSelect(&text, size)) return xl6TextFailure;
    size_t length = 0;
    char* result = xrtRegexEscape(text, &length);
    return xl6TextOwn(result, length);
}

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6RegexHandlePolicyV1Get,xl6RegexDestroyV1)
