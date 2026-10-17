#ifndef XL6_FUTURE_VALUE_H
#define XL6_FUTURE_VALUE_H
#include "xl6_code_lease.h"
#include "xl6_future_payload.h"
#include "xl6_capture_v2.h"
/* By-value, immutable descriptor. Runtime states COPY these fields, never
 * retain module-local descriptor storage or infer a payload from a type name.
 * Native is either NULL or one permanent resident native-leaf capability. */
typedef enum XL6_FutureValueKindV1 {
    XL6_FUTURE_VALUE_INVALID=0, XL6_FUTURE_VALUE_VOID,
    XL6_FUTURE_VALUE_TRIVIAL, XL6_FUTURE_VALUE_STRING,
    XL6_FUTURE_VALUE_XVALUE, XL6_FUTURE_VALUE_ERROR,
    XL6_FUTURE_VALUE_CLOSURE, XL6_FUTURE_VALUE_FUTURE,
    XL6_FUTURE_VALUE_GENERATOR, XL6_FUTURE_VALUE_NATIVE
} XL6_FutureValueKindV1;
typedef struct XL6_FutureValueProfileV1 {
    size_t size;
    XL6_FutureValueKindV1 kind;
    size_t valueSize;
    uint64_t type;
    const XL6_NativeCaptureProfileV1* native;
} XL6_FutureValueProfileV1;
XL6_CODE_LEASE_API bool xl6FutureValueProfileValidV1(XL6_FutureValueProfileV1);
/* Copy borrows one initialized C payload slot. False is authoritative even
 * when diagnostics cannot allocate. Output starts NULL and remains NULL on
 * failure. VOID succeeds with NULL; all other kinds own an independent cell. */
XL6_CODE_LEASE_API bool xl6FutureValueCopyV1(XL6_FutureValueProfileV1,const void*,void**);
XL6_CODE_LEASE_API void xl6FutureValueDropV1(XL6_FutureValueProfileV1,void*);
XL6_CODE_LEASE_API bool xl6FutureValuePublishV1(XL6_FutureValueProfileV1,struct xpromise*,void*);
#endif
