#include "xl6_tcp_internal.h"
#include "xl6_net_bytes_internal.h"
int64_t xl6TcpStreamSendRefV1(void* receiver,void* input,int64_t size,int64_t offset)
{
    XL6_TcpScopeV1 scope={0};xbytesview view={0};int64_t result=XNET_RESULT_ERROR;
    if(!xl6NetBytesRangeV1(input,size,offset,&view))goto done;
    if(!xl6ExecutionTcpEnterInternalV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))goto done;
    /* Native send retains the immutable payload itself, not this call's
     * language wrapper, a generated release callback or a borrowed slice. */
    result=xrtNetStreamSendBytes(scope.stream,input,(size_t)offset,view.Size);
done:
    if(scope.execution.resource)xl6ExecutionTcpLeaveInternalV1(&scope);
    if(result==XNET_RESULT_ERROR)xl6TcpFailedV1();
    return result;
}
