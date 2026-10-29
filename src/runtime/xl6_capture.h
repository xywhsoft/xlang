#ifndef XL6_CAPTURE_H
#define XL6_CAPTURE_H
#include "xl6_code_lease.h"

/* Closed physical capture program, produced from verified transfer/destroy/
 * trace ABIs. No producer callback is an ownership certificate. TRIVIAL owns
 * nothing; an opaque/native owning pointer must never be described as trivial.
 * Descriptors are immutable, separate from data, and protected by the frame's
 * actual code lease. These are byte offsets, not source symbols or C types. */
typedef enum XL6_CaptureKind {
    XL6_CAPTURE_TRIVIAL = 1, XL6_CAPTURE_STRING, XL6_CAPTURE_XVALUE,
    XL6_CAPTURE_FUTURE, XL6_CAPTURE_CLOSURE, XL6_CAPTURE_ERROR,
    XL6_CAPTURE_CELL, XL6_CAPTURE_VALUE_ITERATOR, XL6_CAPTURE_GENERATOR
} XL6_CaptureKind;
typedef struct XL6_CaptureSlot {
    XL6_CaptureKind kind;
    size_t offset, bytes;
} XL6_CaptureSlot;
typedef struct XL6_CaptureLayoutV1 {
    size_t size, bytes, count;
    const XL6_CaptureSlot* slots;
} XL6_CaptureLayoutV1;
typedef struct XL6_CaptureRuntimeV1 {
    /* Validate the entire layout before allocation, acquisition or callbacks.
     * Private producers initialize every slot before fallible capture. */
    bool (*Validate)(const XL6_CaptureLayoutV1*, size_t);
    /* Borrowed inspection requires the containing frame's quiescence. Every
     * physical child still requires independent resolver admission. */
    bool (*Trace)(const XL6_CaptureLayoutV1*, const void*, XL6_ModuleOwnershipVisit, void*);
    /* Reverse declaration order, typed null detach before release, first error
     * preserved. Only slot writes hold mutation; release/finalizers do not.
     * Caller marks its frame active through this entire operation and must not
     * inherit mutation/freeze. Repeated cleanup sees the already-empty slots.
     * Does not run the language callback/finally or free the frame allocation. */
    void (*Cleanup)(const XL6_CaptureLayoutV1*, void*);
} XL6_CaptureRuntimeV1;
XL6_CODE_LEASE_API const XL6_CaptureRuntimeV1* xl6CaptureRuntimeV1Get(void);

/* Continuation captures have sparse lifetimes. Slots stay in physical frame
 * order for tracing; cleanup names the exact verified destroy-action order.
 * A trivial slot (including a protected loan) has initialized == SIZE_MAX and
 * never owns a child. Every owning slot has its own unsigned-char 0/1 flag.
 * Layout/slots/order are immutable and disjoint from the activation, protected
 * by its real code lease. This describes payload, not a new graph identity or
 * permission to collect an opaque frame/control node. */
typedef struct XL6_ActivationCaptureSlot {
    XL6_CaptureSlot value;
    size_t initialized;
} XL6_ActivationCaptureSlot;
typedef struct XL6_ActivationCaptureLayoutV1 {
    size_t size, bytes, count;
    const XL6_ActivationCaptureSlot* slots;
    size_t cleanupCount;
    const size_t* cleanup;
} XL6_ActivationCaptureLayoutV1;
struct xl6_foreign_error_result;
typedef struct XL6_ActivationCaptureRuntimeV1 {
    bool (*Validate)(const XL6_ActivationCaptureLayoutV1*, size_t);
    bool (*Trace)(const XL6_ActivationCaptureLayoutV1*, const void*, XL6_ModuleOwnershipVisit, void*);
    /* Requires exclusive/active ownership, outside freeze/mutation. Each
     * initialized flag and its typed owning pointer detach in one mutation;
     * release/finally runs afterward. All steps run, preserving even a first
     * failure with no allocated diagnostic, and leaving ambient errors intact.
     * Does not free frame storage or run the containing machine's source CFG. */
    struct xl6_foreign_error_result (*Cleanup)(const XL6_ActivationCaptureLayoutV1*, void*,
        struct xl6_foreign_error_result);
} XL6_ActivationCaptureRuntimeV1;
XL6_CODE_LEASE_API const XL6_ActivationCaptureRuntimeV1* xl6ActivationCaptureRuntimeV1Get(void);
#endif
