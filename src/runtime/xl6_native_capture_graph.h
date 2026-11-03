#ifndef XL6_NATIVE_CAPTURE_GRAPH_H
#define XL6_NATIVE_CAPTURE_GRAPH_H
#include "xl6_capture_v2.h"

/* A permanent resident native storage capability. Its deliberately larger
 * V1 prefix (with NULL Type/DetachCleanup) is rejected by every legacy leaf
 * consumer. The profile is metadata, not a code-residency acquisition.
 *
 * All-zero storage is a typed empty value, safe for Trace and repeated Drop.
 * CopyFresh writes into a typed empty destination, owns independently, and
 * leaves it empty on failure. Trace extracts EACH actual owning reference
 * from the private slot; it does not inspect child state or call its graph
 * methods. Detach empties the slot/live flag before arbitrary cleanup and
 * consumes that owner even when returning false without a diagnostic. */
typedef struct XL6_NativeGraphCaptureProfileV1 {
    XL6_NativeCaptureProfileV1 prefix;
    uint64_t abiTag;
    size_t bytes,alignment;
    bool (*CopyFresh)(void*,const void*);
    bool (*Trace)(const void*,XL6_ModuleOwnershipVisit,void*);
    bool (*Detach)(void*,unsigned char*);
} XL6_NativeGraphCaptureProfileV1;
#define XL6_NATIVE_GRAPH_CAPTURE_V1_TAG UINT64_C(0x584C364E47435031)
#define XL6_CAPTURE_NATIVE_GRAPH_V3 ((XL6_CaptureKind)11)

XL6_CODE_LEASE_API const XL6_NativeGraphCaptureProfileV1* xl6NativeGraphCaptureProfileV1Get(const XL6_NativeCaptureProfileV1*);
static inline bool xl6NativeGraphCaptureTraceV1(const XL6_NativeCaptureProfileV1* prefix,
    const void* slot,XL6_ModuleOwnershipVisit visit,void* context)
{
    const XL6_NativeGraphCaptureProfileV1* profile=xl6NativeGraphCaptureProfileV1Get(prefix);
    return profile && slot && visit && profile->Trace(slot,visit,context);
}
#endif
