#ifndef XL6_EXECUTION_TCP_INTERNAL_H
#define XL6_EXECUTION_TCP_INTERNAL_H
#include "xl6_execution_resource_internal.h"
#include "xl6_execution_tcp.h"
typedef struct XL6_TcpCreateOptionsV1 {
    XL6_ExecutionResourceV1* engine;
    const xnetaddr* remote;
    uint32_t worker;
    const xnetstreamconfig* stream;
    const xnetlistenconfig* listener;
    xnetstream* accepted;
    XL6_ExecutionResourceV1* accepting; /* retained parent Listener, no operation yet */
} XL6_TcpCreateOptionsV1;
typedef struct XL6_TcpScopeV1 {
    XL6_ExecutionScopeV1 execution;
    xnetstream* stream;
    xnetlistener* listener;
} XL6_TcpScopeV1;
/* Physical scope, same thread, not copyable. Returned native pointers are
 * call-only borrows; every successful Enter must reach Leave. */
XL6_EXECUTION_INTERNAL bool xl6ExecutionTcpEnterInternalV1(
    XL6_ExecutionResourceV1*,XL6_ExecutionKindV1,bool,XL6_TcpScopeV1*);
XL6_EXECUTION_INTERNAL void xl6ExecutionTcpLeaveInternalV1(XL6_TcpScopeV1*);
#endif
