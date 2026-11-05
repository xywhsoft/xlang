#ifndef XL6_EXECUTION_TCP_DIAL_INTERNAL_H
#define XL6_EXECUTION_TCP_DIAL_INTERNAL_H
#include "xl6_execution_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
/* Trusted same-resident submission only, invoked synchronously after real
 * parent/control scopes are entered. Arguments are borrowed for this call,
 * never retained; the native constructor copies them before acceptance.
 * No Proxy provider is pulled into a TCP-only product through this helper. */
typedef xfuture* (*XL6_TcpNativeDialSubmitV1)(xnetengine*,xnetresolver*,const void*);
XL6_EXECUTION_INTERNAL xfuture* xl6ExecutionTcpDialSubmitInternalV1(
    XL6_ExecutionResourceV1*,bool,XL6_TcpNativeDialSubmitV1,const void*);
#endif
