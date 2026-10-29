#ifndef XL6_TCP_DIAL_SYNC_INTERNAL_H
#define XL6_TCP_DIAL_SYNC_INTERNAL_H
#include "xl6_execution_tcp_internal.h"
XL6_EXECUTION_INTERNAL bool xl6TcpDialMayWaitInternalV1(void*);
XL6_EXECUTION_INTERNAL void* xl6TcpDialFinishUntilInternalV1(XL6_ExecutionResourceV1*,xfuture*,uint64_t,xcancel*);
#endif
