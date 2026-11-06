#ifndef XL6_VALUE_READ_VIEW_H
#define XL6_VALUE_READ_VIEW_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Allocation-free read scope for wire Array/Object containers. Caller owns
 * the real receiver for the complete synchronous scope and serializes access
 * to it. Begin borrows, never retains/clones/boxes any value or backing. The
 * receiver is BUSY until End: mutation, retain, clone and release through that
 * shell fail; independently owned aliases retain normal COW behavior. Children
 * and key/text bytes are borrowed only until End; open child containers before
 * invoking code that could change their contents. No suspend, thread transfer,
 * copying a live scope, or accessing raw receiver APIs while it is active.
 * Storage starts zeroed. End clears it and is idempotent on a cleared scope.
 * Scope guards add no fields or checks to ordinary values or operations. */
#ifndef XRT_VALUE_VIEW_V1_DEFINED
#define XRT_VALUE_VIEW_V1_DEFINED
typedef struct xvalueviewv1 {
    const struct xvalueviewv1* Self;
    const xvalue* Value;
    const void* Backing;
    size_t Count;
    xvaluetype Type;
} xvalueviewv1;
XRT_API bool xrtValueViewBeginV1(const xvalue* pValue, xvalueviewv1* pView);
XRT_API bool xrtValueViewEndV1(xvalueviewv1* pView);
XRT_API size_t xrtValueViewCountV1(const xvalueviewv1* pView);
/* At is checked, zero-based; Object keys retain insertion order. Find is
 * Object-only, with exact byte keys. Missing Find returns NULL without error. */
XRT_API const xvalue* xrtValueViewAtV1(const xvalueviewv1* pView, size_t iIndex, xstrview* pKey);
XRT_API const xvalue* xrtValueViewFindV1(const xvalueviewv1* pView, xstrview Key);
#endif


#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL_VALUE_READ_API __declspec(dllexport)
#else
#define XL_VALUE_READ_API
#endif
/* Borrowed scalar: no owning shell, no conversion or text allocation. ref is
 * present for a source Value; native POD/text slots use the inline payload.
 * Container children must be opened while their parent scope is still live. */
typedef struct XL_ValueBorrowV1 {
    xvaluetype kind;
    const xvalue* ref;
    union { bool boolean; int64_t integer; uint64_t unsignedInteger;
        double number; xstrview text; xbytesview bytes; } data;
} XL_ValueBorrowV1;
/* All fields are private runtime state. Zero initialize and never copy a live
 * scope. Same synchronous native call; no await, yield or thread transfer.
 * Exact resident policies authenticate native owners before payload access.
 * Begin/End/At/Find copy no elements, create no shells and allocate nothing.
 * Every successful Begin requires End on ALL paths, including errors. */
typedef struct XL_ValueReadV1 {
    struct XL_ValueReadV1* self;
    const xvalue* source;
    void* owner;
    void* storage;
    uint64_t identity, elementIdentity, slotIdentity;
    unsigned transfer;
    size_t slotSize, count;
    const struct xrttype* slotType;
    xvalueviewv1 wire;
    xvaluetype kind;
    unsigned mode;
} XL_ValueReadV1;
XL_VALUE_READ_API XL_ValueBorrowV1 xlValueBorrowV1(const xvalue*);
XL_VALUE_READ_API bool xlValueReadBeginV1(XL_ValueBorrowV1,XL_ValueReadV1*);
XL_VALUE_READ_API bool xlValueReadEndV1(XL_ValueReadV1*);
XL_VALUE_READ_API XL_ValueBorrowV1 xlValueReadAtV1(const XL_ValueReadV1*,size_t,xstrview*);
XL_VALUE_READ_API XL_ValueBorrowV1 xlValueReadFindV1(const XL_ValueReadV1*,xstrview);
/* Only explicit ownership conversion allocates/copies a native leaf. Use this
 * for a returned result that must outlive the read scope, never for reading. */
XL_VALUE_READ_API xvalue* xlValueBorrowCopyV1(XL_ValueBorrowV1);
#ifdef __cplusplus
}
#endif
#endif
