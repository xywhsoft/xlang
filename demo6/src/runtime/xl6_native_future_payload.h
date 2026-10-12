#ifndef XL6_NATIVE_FUTURE_PAYLOAD_H
#define XL6_NATIVE_FUTURE_PAYLOAD_H
#include "xl6_capture_v2.h"
struct xpromise;
struct xfuturepayloadownershipv1;
struct xrtownershipadapterv1;
/* One resident allocation contains its immutable native profile and an aligned
 * typed value. Allocate returns the value address (also the Future payload),
 * initially empty. Producers move into it; Resolve consumes it only on success.
 * This is not erased xvalue storage or a module-local destructor. The profile
 * must be the same permanent, immutable native-leaf capability as CaptureV2.
 * Copy borrows a retained Future payload, owns independently and checks exact
 * profile identity. Its aligned output must be empty; false is authoritative
 * even without a diagnostic. Both failure and Drop detach before releasing.
 * All calls require genuine allocations from this runtime, not arbitrary C
 * pointers. The embedding XRT/runtime outlives every accepted payload. */
typedef struct XL6_NativeFuturePayloadRuntimeV1 {
    void* (*Allocate)(const XL6_NativeCaptureProfileV1*);
    void (*Drop)(void*,void*);
    bool (*ResolveOwned)(struct xpromise*,void*);
    bool (*Copy)(const void*,const XL6_NativeCaptureProfileV1*,void*);
    const struct xfuturepayloadownershipv1* (*Policy)(void);
    const struct xrtownershipadapterv1* (*OwnershipAdapter)(struct xrtownershipref);
} XL6_NativeFuturePayloadRuntimeV1;
XL6_CODE_LEASE_API const XL6_NativeFuturePayloadRuntimeV1* xl6NativeFuturePayloadRuntimeV1Get(void);
#endif
