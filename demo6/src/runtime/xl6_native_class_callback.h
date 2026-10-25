#ifndef XL6_NATIVE_CLASS_CALLBACK_H
#define XL6_NATIVE_CLASS_CALLBACK_H
#include "xl6_native_class_result.h"
#include "xl6_closure.h"
/* Compiler-sealed checked (native class) -> nullable native class profile.
 * The returned ordinary V2 closure owns the real creator lease, target closure
 * and copied field names. Its graph is the existing closed capture graph, not
 * an opaque callback box. No source-class placeholder or generated factory. */
XL6_CODE_LEASE_API XL6_Closure* xl6NativeClassCallbackCreateV1(
    XL6_NativeClassCallbackContextV1,XL6_Closure*);
/* Valid driver consumes *argument on every path. On success *result is an
 * independent native-handle owner, or NULL for the optional rejection result.
 * Invalid driver/output slots consume nothing: no unknown drop is callable.
 * The caller owns the driver throughout invocation, including rollback.
 * This checked result does not itself publish a native callback failure. */
XL6_CODE_LEASE_API bool xl6NativeClassCallbackInvokeTakeV1(
    XL6_Closure*,void** argument,struct xvalue** result);
#endif
