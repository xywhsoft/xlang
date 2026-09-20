#ifndef XL6_GENERATOR_H
#define XL6_GENERATOR_H
#include <stdint.h>
#include "xl6_code_lease.h"
#include "xl6_capture.h"

typedef struct xlir_generator XL6_Generator;
typedef void (*XL6_GeneratorFree)(void*);
typedef bool (*XL6_GeneratorResume)(XL6_Generator*);
typedef struct xlir_generator_step { bool ok; bool value; } XL6_GeneratorStep;
struct xl6_foreign_error_result;

/* Private versioned layout. A producer may READ frame/state/closing and the
 * verifier fixes every state and payload transfer. All lifecycle writes go
 * through the resident API. A V1 generator has one owning slot; native aliases
 * borrow it. V2 has explicit real references, but neither version permits
 * concurrent invocation or racing payload access. */
struct xlir_generator {
    void* frame;
    XL6_GeneratorResume resume;
    XL6_GeneratorFree frame_free, item_free;
    XL6_CodeContextTrace item_trace;
    XL6_CodeLease* lease;
    size_t item_size;
    uint32_t state;
    bool has_item, done, closing, running;
    max_align_t item_alignment;
    unsigned char item[];
};
typedef struct XL6_GeneratorRuntimeV1 {
    /* Frame storage has a resident private header. Create takes only storage
     * from FrameAllocate. Failure leaves it owned by the producer; success
     * consumes it. FrameFree only disposes detached storage, after the verified
     * producer callback has dropped every initialized payload. */
    void* (*FrameAllocate)(size_t);
    void (*FrameFree)(void*);
    XL6_Generator* (*Create)(const XL6_CodeOwner*, void*, XL6_GeneratorResume,
        XL6_GeneratorFree, size_t, XL6_GeneratorFree, XL6_CodeContextTrace, XL6_CodeContextTrace);
    struct xrtownershipref (*Ownership)(const XL6_Generator*);
    XL6_GeneratorStep (*Next)(XL6_Generator*);
    bool (*ItemGet)(XL6_Generator*, void*, size_t, bool);
    struct xl6_foreign_error_result (*Close)(XL6_Generator*);
    void (*Destroy)(XL6_Generator*);
    /* Producer callback operations; the resident outer call is already active.
     * Finish detaches the frame before invoking its destructor outside mutation.
     * State zero is unstarted; verified nonzero states resume close/finally. */
    bool (*Put)(XL6_Generator*, const void*, size_t);
    void (*SetState)(XL6_Generator*, uint32_t);
    void (*Finish)(XL6_Generator*);
} XL6_GeneratorRuntimeV1;
/* Real callback addresses reside in the host. Code leases cover descriptor,
 * resume/item/frame callbacks and the return tail; an active call can inherit
 * its accepted lease after owner Close. Callbacks/waits do not hold mutation.
 * Control AND frame inspection refuse while active or physically retiring.
 * This does not certify arbitrary frame children or implement module cycles. */
XL6_CODE_LEASE_API const XL6_GeneratorRuntimeV1* xl6GeneratorRuntimeV1Get(void);
/* V2 describes every payload, with no producer trace/drop callbacks. The
 * language still transfers one generator owner; explicit native Retain and
 * collector Hold create real additional references. Destroy releases one
 * reference and the last release closes normally. Close explicitly terminates
 * the source machine independently of other references. Same-instance V1
 * operations accept V2 controls, but legacy opaque controls are not certified.
 *
 * FrameAllocate leaves storage private: initialize all owning slots/flags
 * before fallible capture. Create consumes it only on success; otherwise use
 * FrameDiscard under the caller's code entry lease. Published frames acquire
 * their own real shared code lease and remain alive through independent pins.
 * Advance/close/data access require external exclusive access. Retain only
 * extends lifetime, not permission to race a running callback. Each V2
 * invocation has its own real control reference; callbacks may retain/release
 * separately owned references, including resurrection during normal close. */
typedef struct XL6_GeneratorRuntimeV2 {
    void* (*FrameAllocate)(size_t, const XL6_ActivationCaptureLayoutV1*);
    void (*FrameDiscard)(void*);
    XL6_Generator* (*Create)(const XL6_CodeOwner*, void*, XL6_GeneratorResume, const XL6_CaptureLayoutV1*);
    XL6_Generator* (*Retain)(XL6_Generator*);
    void* (*FrameRetain)(void*);
    void (*FrameRelease)(void*);
    struct xrtownershipref (*FrameOwnership)(const void*);
} XL6_GeneratorRuntimeV2;
XL6_CODE_LEASE_API const XL6_GeneratorRuntimeV2* xl6GeneratorRuntimeV2Get(void);
struct xrtownershipadapterv1;
struct xrtownershippreparationv1;
/* Independently recognizes the control OR the actual frame identity. Prepare
 * runs normal close/finally, never forced edge deletion. All children still
 * need their own admission. Code leases survive to the last physical Drop. */
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6GeneratorOwnershipAdapterV1(
    struct xrtownershipref, const struct xrtownershippreparationv1**);
#endif
