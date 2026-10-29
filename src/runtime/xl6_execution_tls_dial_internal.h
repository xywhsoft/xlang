#ifndef XL6_EXECUTION_TLS_DIAL_INTERNAL_H
#define XL6_EXECUTION_TLS_DIAL_INTERNAL_H
#include "xl6_execution_tls_internal.h"
#include "xl6_tls_role_context_internal.h"
typedef struct XL6_TlsNativeDialArgumentsV1 {
    const char* host;
    uint16_t port;
    const xnetproxy* proxy;
    const xtlsclientconfig* client;
    const xtlsdialconfig* config;
} XL6_TlsNativeDialArgumentsV1;
/* Trusted resident functions, exact types, call-only arguments. No generated
 * callbacks or borrowed language input survive native acceptance. */
typedef xfuture* (*XL6_TlsNativeDialSubmitV1)(xnetengine*,xnetresolver*,
    XL6_ExecutionResourceV1*,const xtlsstreamownershipv1*,const XL6_TlsNativeDialArgumentsV1*);
XL6_EXECUTION_INTERNAL xfuture* xl6ExecutionTlsDialSubmitV1(XL6_ExecutionResourceV1*,
    XL6_TlsNativeDialSubmitV1,const XL6_TlsNativeDialArgumentsV1*);
#endif
