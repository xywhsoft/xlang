#ifndef XL6_TCP_FUTURE_INTERNAL_H
#define XL6_TCP_FUTURE_INTERNAL_H
#include "xl6_execution_tcp_internal.h"
#include "xl6_runtime_services.h"
#include "xl6_native_handle.h"
#include "xl6_native_class_abi.h"
typedef struct XL6_TcpClassCaptureV1 XL6_TcpClassCaptureV1;
typedef bool (*XL6_TcpPromoteResidentV1)(XL6_ExecutionResourceV1*);
struct xfutureresult;
typedef bool (*XL6_TcpClassMapResidentV1)(const xvalue*,const xvalue*,xstrview,
    const struct xfutureresult*,xvalue**);
/* Same-resident providers only. Promotion is permanent runtime code, never
 * a generated callback or borrowed module policy. Prepare owns real values,
 * seals/copies the class field, and submits no native work. */
XL6_EXECUTION_INTERNAL XL6_TcpClassCaptureV1* xl6TcpClassCapturePrepareInternalV1(
    XL6_NativeClassFieldContextV1,void*,const xvalue*,const XL6_NativeHandlePolicyV1*,
    XL6_TcpPromoteResidentV1,bool,XL6_ExecutionResourceV1**);
XL6_EXECUTION_INTERNAL struct xfuture* xl6TcpClassCapturePublishInternalV1(
    XL6_RuntimeServicesV1,XL6_TcpClassCaptureV1*,struct xfuture*);
XL6_EXECUTION_INTERNAL struct xfuture* xl6TcpClassCaptureFailInternalV1(
    XL6_TcpClassCaptureV1*,struct xfuture*);
/* Private resident result mapping: expected owns a valid prebuilt class
 * field; only the permanent decoder may replace it before publication.
 * The callback is never a generated function or borrowed module context. */
XL6_EXECUTION_INTERNAL XL6_TcpClassCaptureV1* xl6TcpValueClassCapturePrepareInternalV1(
    XL6_NativeClassFieldContextV1,void*,const xvalue*,const XL6_NativeHandlePolicyV1*,
    const XL6_NativeHandlePolicyV1*,XL6_TcpClassMapResidentV1,void**);
#endif
