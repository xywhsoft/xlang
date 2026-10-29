#ifndef XL6_EXECUTION_UDP_INTERNAL_H
#define XL6_EXECUTION_UDP_INTERNAL_H
#include "xl6_execution_resource_internal.h"
#include "xl6_execution_udp.h"
typedef struct XL6_UdpCreateOptionsV1 {
    XL6_ExecutionResourceV1* engine;
    const xnetaddr* local;
    const xnetaddr* peer;
    const xnetudpconfig* config;
    uint64_t affinity;
} XL6_UdpCreateOptionsV1;
typedef struct XL6_UdpScopeV1 {
    XL6_ExecutionScopeV1 execution;
    xnetudp* udp; /* physical call-only borrow; never store beyond Leave */
} XL6_UdpScopeV1;
XL6_EXECUTION_INTERNAL bool xl6ExecutionUdpEnterInternalV1(XL6_ExecutionResourceV1*,bool,XL6_UdpScopeV1*);
XL6_EXECUTION_INTERNAL void xl6ExecutionUdpLeaveInternalV1(XL6_UdpScopeV1*);
#endif
