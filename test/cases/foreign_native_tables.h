#ifndef FOREIGN_NATIVE_TABLES_H
#define FOREIGN_NATIVE_TABLES_H
#include <__xl_runtime/xl6_native_callback.h>
#include <__xl_runtime/xl6_closure_capture_v4.h>
#include <__xl_runtime/xl6_foreign_view_capture.h>
#include <__xl_runtime/xl6_native_graph_future_payload.h>
#include <__xl_runtime/xl6_foreign_view_value.h>
#include <__xl_runtime/xl6_foreign_view_sequence.h>
#ifdef __cplusplus
extern "C" {
#endif
int foreign_native_tables_probe(const XL6_NativeCallbackRuntimeV1*,
    const XL6_ClosureRuntimeV3*, const XL6_NativeCaptureProfileV1*,
    const XL6_NativeFuturePayloadRuntimeV1*, const XL6_OwnershipDomainV1*,
    const XL6_OwnershipDomainV1*);
#ifdef __cplusplus
}
#endif
#endif
