#include "xl6_tcp_internal.h"
#include "xl6_net_buffer_internal.h"
int64_t xl6TcpStreamSendChainMoveV1(void* receiver,void* input)
{
    XL6_NetBufferV1* buffer=input;XL6_NetBufferGuardV1 guard={0};XL6_TcpScopeV1 scope={0};
    int64_t result=XNET_RESULT_ERROR;
    bool ok=xl6NetBufferEnterV1(buffer,&guard)&&xl6NetBufferCanAppendV1(buffer,0);
    if(ok)ok=xl6ExecutionTcpEnterInternalV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope);
    /* The buffer guard excludes both competing mutations and Freeze for the
     * exact native take/rollback. No borrowed source address reaches a queue. */
    if(ok)result=xrtNetStreamSendBuffer(scope.stream,&buffer->native);
    if(scope.execution.resource)xl6ExecutionTcpLeaveInternalV1(&scope);
    if(guard.owner)xl6NetBufferLeaveV1(&guard);
    if(result==XNET_RESULT_ERROR)xl6TcpFailedV1();
    return result;
}
