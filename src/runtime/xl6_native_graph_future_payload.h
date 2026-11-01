#ifndef XL6_NATIVE_GRAPH_FUTURE_PAYLOAD_H
#define XL6_NATIVE_GRAPH_FUTURE_PAYLOAD_H
#include "xl6_native_future_payload.h"
#include "xl6_native_capture_graph.h"
/* Same operation-table shape, independently admitted graph payload policy.
 * Only graph profiles and genuine allocations from this runtime are accepted.
 * Copy rejects a non-graph expected profile before reading its private header;
 * mixing allocators with a fabricated matching profile is a C contract violation.
 * Output of Copy must be typed empty.
 * Native policy Drop marks a callback boundary and preserves a separate
 * cleanup-failure bit. Creator-thread deferred cleanup is drained by the
 * surrounding complete native-call/checked host boundary after C returns. */
XL6_CODE_LEASE_API const XL6_NativeFuturePayloadRuntimeV1* xl6NativeGraphFuturePayloadRuntimeV1Get(void);
XL6_CODE_LEASE_API const struct XL6_OwnershipDomainV1* xl6OwnershipDomainNativeGraphFuturePayloadV1Get(void);
#endif
