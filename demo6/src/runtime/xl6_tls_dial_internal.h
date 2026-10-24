#ifndef XL6_TLS_DIAL_INTERNAL_H
#define XL6_TLS_DIAL_INTERNAL_H
#include "xl6_tls_class_future_internal.h"
#include "xl6_execution_tls_dial.h"
#include "xl6_tls_dial_options_internal.h"
#include "xl6_tls_role_context_internal.h"
/* Call-only decoded inputs. Host is the only temporary owner; native Dial
 * copies it, names, ALPN and configuration before accepting the operation. */
typedef struct XL6_TlsDialInputV1 {
    char* host;
    uint16_t port;
    xtlsclientconfig client;
    xtlsdialconfig config;
} XL6_TlsDialInputV1;
typedef xfuture* (*XL6_TlsDialSourceV1)(XL6_ExecutionResourceV1*,const XL6_TlsDialInputV1*,const xnetproxy*);
XL6_EXECUTION_INTERNAL bool xl6TlsDialInputReadV1(xstrview,int64_t,int64_t,const void*,const void*,bool,XL6_TlsDialInputV1*);
XL6_EXECUTION_INTERNAL xfuture* xl6TlsDialNativeSourceV1(XL6_ExecutionResourceV1*,const XL6_TlsDialInputV1*,const xnetproxy*);
XL6_EXECUTION_INTERNAL xfuture* xl6TlsProxyDialNativeSourceV1(XL6_ExecutionResourceV1*,const XL6_TlsDialInputV1*,const xnetproxy*);
XL6_EXECUTION_INTERNAL xfuture* xl6TlsDialClassSourceV1(XL6_ExecutionServicesV1,void*,void*,void*,const XL6_TlsDialInputV1*,const xnetproxy*,XL6_TlsDialSourceV1);
XL6_EXECUTION_INTERNAL void* xl6TlsDialSyncV1(XL6_ExecutionServicesV1,void*,void*,const XL6_TlsDialInputV1*,const xnetproxy*,XL6_TlsDialSourceV1,uint64_t,xcancel*);
#endif
