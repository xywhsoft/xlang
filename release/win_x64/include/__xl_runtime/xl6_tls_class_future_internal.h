#ifndef XL6_TLS_CLASS_FUTURE_INTERNAL_H
#define XL6_TLS_CLASS_FUTURE_INTERNAL_H
#include "xl6_tls_io_internal.h"
/* Only trusted permanent resident promotion functions, never generated or
 * language callbacks. The real callback frame owns the result class lease. */
typedef bool (*XL6_TlsResultPromoteV1)(XL6_ExecutionResourceV1*);
XL6_EXECUTION_INTERNAL void* xl6TlsClassFuturePrepareV1(XL6_NativeClassResultContextV1,
    void*,const XL6_NativeHandlePolicyV1*,XL6_TlsResultPromoteV1);
/* Consumes the owning creator on success AND allocation failure, exactly as
 * NativeHandleTake; leaves it unchanged only for invalid call arguments. */
XL6_EXECUTION_INTERNAL bool xl6TlsClassFutureBindV1(void*,XL6_ExecutionResourceV1**);
#endif
