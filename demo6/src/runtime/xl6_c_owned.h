#ifndef XL6_C_OWNED_H
#define XL6_C_OWNED_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif

/* Compiler-private owners. Ordinary string/bytes and XRT layouts are unchanged.
 * Reads use Data/Size directly; only acquisition and cleanup inspect Owner. */
typedef void (*XL6_CReleaseV1)(void*);
typedef struct XL6_COwnerV1 {
    xatomic64 References;
    void* Allocation;
    XL6_CReleaseV1 Release;
} XL6_COwnerV1;
typedef struct XL6_COwnedV1 {
    void* Data;
    size_t Size;
    XL6_COwnerV1* Owner;
} XL6_COwnedV1;

static inline void xl6COwnedDropV1(XL6_COwnedV1* value)
{
    XL6_COwnedV1 owned=*value;
    *value=(XL6_COwnedV1){0};
    if(owned.Owner) {
        if(xrtAtomic64FetchSub(&owned.Owner->References,1,XMEMORY_ACQ_REL)==1) {
            void* allocation=owned.Owner->Allocation;
            XL6_CReleaseV1 release=owned.Owner->Release;
            xrtFree(owned.Owner);
            release(allocation);
        }
    } else xrtFree(owned.Data); /* ordinary string committed into a C-capable slot */
}
static inline void xl6COwnedDropValueV1(XL6_COwnedV1 value)
{
    xl6COwnedDropV1(&value);
}
static inline XL6_COwnedV1 xl6COwnedTakeV1(void* data,size_t size,XL6_CReleaseV1 release)
{
    if(!data)return (XL6_COwnedV1){0};
    XL6_COwnerV1* owner=(XL6_COwnerV1*)xrtMalloc(sizeof(*owner));
    if(!owner) { release(data); return (XL6_COwnedV1){0}; }
    owner->Allocation=data;owner->Release=release;
    xrtAtomic64Init(&owner->References,1);
    return (XL6_COwnedV1){data,size,owner};
}
/* One independent owner; shared C storage never copies payload bytes. Ordinary
 * strings stored after a write retain their existing value-copy semantics. */
static inline XL6_COwnedV1 xl6COwnedCopyV1(XL6_COwnedV1 value)
{
    if(value.Owner) { xrtAtomic64FetchAdd(&value.Owner->References,1,XMEMORY_RELAXED); return value; }
    if(!value.Data)return value;
    char* copy=xrtStrDupN((const char*)value.Data,value.Size);
    return (XL6_COwnedV1){copy,copy ? value.Size : 0,NULL};
}
static inline XL6_COwnedV1 xl6CStringFromStringV1(xstrview value)
{
    return (XL6_COwnedV1){(void*)value.Data,value.Size,NULL};
}
static inline void xl6COwnedReleaseBytesV1(void* value) { xrtValueRelease((xvalue*)value); }
static inline XL6_COwnedV1 xl6CBytesFromBytesV1(xvalue* value)
{
    xbytesview view={0};
    if(!xrtValueGetBytes(value,&view)) { xrtValueRelease(value); return (XL6_COwnedV1){0}; }
    XL6_COwnedV1 result=xl6COwnedTakeV1(value,view.Size,xl6COwnedReleaseBytesV1);
    if(result.Owner)result.Data=(void*)view.Data;
    return result;
}

/* A successful take owns exactly the original allocation address. NULL is an
 * empty carrier; non-NULL zero-length allocations still require Release. The
 * compiler admits only an exact, named, no-raise C void(void*) releaser. */
static inline XL6_COwnedV1 xl6CStringTakeV1(void* data, XL6_CReleaseV1 release)
{
    return xl6COwnedTakeV1(data,data ? strlen((const char*)data) : 0,release);
}

static inline XL6_COwnedV1 xl6CBytesTakeV1(void* data, int64_t size, XL6_CReleaseV1 release)
{
    if (size < 0 || (uint64_t)size > SIZE_MAX || (!data && size)) {
        /* Ownership transfers on entry, including a rejected length. */
        if (data) release(data);
        xrtSetErrorKind(XERR_ARGUMENT);
        return (XL6_COwnedV1){0};
    }
    return xl6COwnedTakeV1(data,(size_t)size,release);
}

static inline xstrview xl6CStringViewV1(XL6_COwnedV1 value)
{
    return (xstrview){value.Data ? (const char*)value.Data : "", value.Size};
}

/* Consumes the already anchored owner after the length expression finishes.
 * This preserves left-to-right evaluation without leaking on a thrown length. */
static inline bool xl6CBytesSizeTakeV1(XL6_COwnedV1 value,int64_t size,XL6_COwnedV1* output)
{
    *output=(XL6_COwnedV1){0};
    if(size<0 || (uint64_t)size>SIZE_MAX || (!value.Data && size)) {
        xl6COwnedDropV1(&value);
        xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    value.Size=(size_t)size;
    *output=value;
    return true;
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
