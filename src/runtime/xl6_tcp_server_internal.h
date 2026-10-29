#ifndef XL6_TCP_SERVER_INTERNAL_H
#define XL6_TCP_SERVER_INTERNAL_H
#include "xl6_text_value.h"
#include "xl6_execution_tcp_server_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
static inline void xl6TcpServerFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static inline bool xl6TcpServerEnterV1(void* raw,bool submit,XL6_TcpServerScopeV1* scope)
{bool ok=xl6ExecutionTcpServerEnterInternalV1(raw,submit,scope);if(!ok)xl6TcpServerFailedV1();return ok;}
#endif
