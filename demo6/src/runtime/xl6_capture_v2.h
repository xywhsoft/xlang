#ifndef XL6_CAPTURE_V2_H
#define XL6_CAPTURE_V2_H
#include "xl6_capture.h"
#include "xl6_text_value.h"

/* Permanent resident publisher capability, not a source/foreign type ID.
 * Immutable native owning leaf: typed zero is empty, Copy owns independently,
 * and there are NO language graph edges. DetachCleanup atomically empties the
 * exact typed slot and its optional 0/1 flag before destruction, then releases
 * outside mutation/freeze. It must not allocate on healthy cleanup. Only
 * trusted C/native publishers may supply this record; this is not a sandbox
 * for arbitrary function/descriptor pointers or module-local callbacks. */
typedef struct XL6_NativeCaptureProfileV1 {
    size_t size;
    uint64_t identity;
    const xrttype* (*Type)(void);
    void (*DetachCleanup)(void*,unsigned char*);
} XL6_NativeCaptureProfileV1;
XL6_TEXT_API bool xl6NativeCaptureProfileValidV1(const XL6_NativeCaptureProfileV1*);

/* Trace/Cleanup take the containing owner's actual allocation size, not a
 * size inferred from potentially stale/invalid layout metadata.
 * Legacy values retain their V1 kind and have native==NULL. Kind 10 is new
 * only in this separately versioned descriptor; V1 validators reject it. */
enum { XL6_CAPTURE_NATIVE_LEAF_V2 = 10 };
typedef struct XL6_CaptureSlotV2 {
    XL6_CaptureSlot value;
    const XL6_NativeCaptureProfileV1* native;
} XL6_CaptureSlotV2;
typedef struct XL6_CaptureLayoutV2 {
    size_t size,bytes,count;
    const XL6_CaptureSlotV2* slots;
} XL6_CaptureLayoutV2;
typedef struct XL6_CaptureRuntimeV2 {
    bool (*Validate)(const XL6_CaptureLayoutV2*,size_t);
    bool (*Trace)(const XL6_CaptureLayoutV2*,const void*,size_t,XL6_ModuleOwnershipVisit,void*);
    /* Reverse physical declaration order; checked incoming outcome and ambient
     * error are independent. Invalid metadata refuses without touching payload.
     * All valid drops run; repeated cleanup sees typed empty native slots. */
    struct xl6_foreign_error_result (*Cleanup)(const XL6_CaptureLayoutV2*,void*,size_t,struct xl6_foreign_error_result);
} XL6_CaptureRuntimeV2;
XL6_TEXT_API const XL6_CaptureRuntimeV2* xl6CaptureRuntimeV2Get(void);

typedef struct XL6_ActivationCaptureSlotV2 {
    XL6_CaptureSlotV2 value;
    size_t initialized;
} XL6_ActivationCaptureSlotV2;
typedef struct XL6_ActivationCaptureLayoutV2 {
    size_t size,bytes,count;
    const XL6_ActivationCaptureSlotV2* slots;
    size_t cleanupCount;
    const size_t* cleanup;
} XL6_ActivationCaptureLayoutV2;
typedef struct XL6_ActivationCaptureRuntimeV2 {
    bool (*Validate)(const XL6_ActivationCaptureLayoutV2*,size_t);
    bool (*Trace)(const XL6_ActivationCaptureLayoutV2*,const void*,size_t,XL6_ModuleOwnershipVisit,void*);
    struct xl6_foreign_error_result (*Cleanup)(const XL6_ActivationCaptureLayoutV2*,void*,size_t,struct xl6_foreign_error_result);
} XL6_ActivationCaptureRuntimeV2;
XL6_TEXT_API const XL6_ActivationCaptureRuntimeV2* xl6ActivationCaptureRuntimeV2Get(void);
#endif
