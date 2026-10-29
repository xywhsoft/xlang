#ifndef XL6_EXECUTION_TCP_SERVER_INTERNAL_H
#define XL6_EXECUTION_TCP_SERVER_INTERNAL_H
#include "xl6_execution_resource_internal.h"
#include "xl6_execution_tcp_server.h"
typedef struct XL6_TcpServerCreateOptionsV1 {
    XL6_ExecutionResourceV1* engine;
    const xnetserverconfig* config;
} XL6_TcpServerCreateOptionsV1;
typedef struct XL6_TcpServerScopeV1 {
    XL6_ExecutionScopeV1 execution;
    xnetserver* server;
    XL6_ExecutionResourceV1* engineOwner;
} XL6_TcpServerScopeV1;
XL6_EXECUTION_INTERNAL bool xl6ExecutionTcpServerEnterInternalV1(
    XL6_ExecutionResourceV1*,bool,XL6_TcpServerScopeV1*);
XL6_EXECUTION_INTERNAL void xl6ExecutionTcpServerLeaveInternalV1(XL6_TcpServerScopeV1*);
#endif
