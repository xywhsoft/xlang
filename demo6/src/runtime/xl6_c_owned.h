#ifndef XL6_C_OWNED_H
#define XL6_C_OWNED_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif

/* Local, unique C allocation owner. Language reads only borrow Data; ordinary
 * value boundaries materialize string/bytes. No owner Copy/Clone/box ABI exists.
 * Release uses the target C ABI and cannot raise or outlive its loaded code. */
typedef void (*XL6_CReleaseV1)(void*);
typedef struct XL6_COwnedV1 {
    void* Data;
    size_t Size;
    XL6_CReleaseV1 Release;
} XL6_COwnedV1;

static inline void xl6COwnedDropV1(XL6_COwnedV1* value)
{
    XL6_COwnedV1 owned = *value;
    *value = (XL6_COwnedV1){0};
    if (owned.Data) owned.Release(owned.Data);
}

/* A successful take owns exactly the original allocation address. NULL is an
 * empty carrier; non-NULL zero-length allocations still require Release. The
 * compiler admits only an exact, named, no-raise C void(void*) releaser. */
static inline XL6_COwnedV1 xl6CStringTakeV1(void* data, XL6_CReleaseV1 release)
{
    return (XL6_COwnedV1){data, data ? strlen((const char*)data) : 0, release};
}

static inline XL6_COwnedV1 xl6CBytesTakeV1(void* data, int64_t size, XL6_CReleaseV1 release)
{
    if (size < 0 || (uint64_t)size > SIZE_MAX || (!data && size)) {
        /* Ownership transfers on entry, including a rejected length. */
        if (data) release(data);
        xrtSetErrorKind(XERR_ARGUMENT);
        return (XL6_COwnedV1){0};
    }
    return (XL6_COwnedV1){data, (size_t)size, release};
}

static inline xstrview xl6CStringViewV1(XL6_COwnedV1 value)
{
    return (xstrview){value.Data ? (const char*)value.Data : "", value.Size};
}

static inline xbytesview xl6CBytesViewV1(XL6_COwnedV1 value)
{
    return (xbytesview){(const uint8*)value.Data, value.Size};
}

static inline xstrview xl6CStringCopyV1(XL6_COwnedV1 value)
{
    xstrview view = xl6CStringViewV1(value);
    char* data = xrtStrDupN(view.Data, view.Size);
    return (xstrview){data, data ? view.Size : 0};
}

/* Materialize directly in the final dynamic representation: no intermediate
 * managed string and no allocator-dependent adoption shortcut. */
static inline xvalue* xl6CStringBoxV1(XL6_COwnedV1 value)
{
    return xrtValueString(xl6CStringViewV1(value));
}

static inline xvalue* xl6CBytesCopyV1(XL6_COwnedV1 value)
{
    return xrtValueBytes(xl6CBytesViewV1(value));
}
#endif
