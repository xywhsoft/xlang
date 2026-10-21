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
    XL6_ExecutionResourceV1* resolving; /* optional real Resolver parent */
    bool dialing; /* prepared Dial, including the no-Resolver address-list path */
    bool serverAccept; /* accepting is a real Server, not a Listener */
} XL6_TcpCreateOptionsV1;

typedef enum XL6_TcpPendingKindV1 {
    XL6_TCP_PENDING_NONE_V1,XL6_TCP_PENDING_ACCEPT_V1,XL6_TCP_PENDING_DIAL_V1,
    XL6_TCP_PENDING_SERVER_ACCEPT_V1
} XL6_TcpPendingKindV1;
/* Concrete resident extension, never part of the shared Resource/public ABI.
 * pendingKind is immutable after creation; pointer slots are guarded by the
 * Resource control/mutation protocol, not by borrowed callback aliases. */
typedef struct XL6_TcpStreamResourceV1 {
    XL6_ExecutionResourceV1 base;
    XL6_ExecutionResourceV1 *parent,*accepting,*resolving;
    xatomicptr native,pending;
    xatomic32 prepared;
    XL6_TcpPendingKindV1 pendingKind;
} XL6_TcpStreamResourceV1;
XL6_EXECUTION_INTERNAL bool xl6ExecutionTcpPromotePendingInternalV1(
    XL6_ExecutionResourceV1*,XL6_TcpPendingKindV1);
typedef struct XL6_TcpScopeV1 {
    XL6_ExecutionScopeV1 execution;
    xnetstream* stream;
    xnetlistener* listener;
    XL6_ExecutionResourceV1* engineOwner; /* call-only borrow, protected by receiver scope */
} XL6_TcpScopeV1;
/* Physical scope, same thread, not copyable. Returned native pointers are
 * call-only borrows; every successful Enter must reach Leave. */
XL6_EXECUTION_INTERNAL bool xl6ExecutionTcpEnterInternalV1(
    XL6_ExecutionResourceV1*,XL6_ExecutionKindV1,bool,XL6_TcpScopeV1*);
XL6_EXECUTION_INTERNAL void xl6ExecutionTcpLeaveInternalV1(XL6_TcpScopeV1*);
#endif
