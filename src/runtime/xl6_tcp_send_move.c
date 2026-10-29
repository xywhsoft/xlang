#include "xl6_tcp_internal.h"
#include "xl6_buffer_internal.h"
int64_t xl6TcpStreamSendMoveV1(void* receiver,void* input)
{
    XL6_TcpScopeV1 scope={0};int64_t result=XNET_RESULT_ERROR;
    xbuffer* buffer=xl6BufferChecked(input);xrtownershipscope mutation={0};bool entered=false;
    if(!buffer)goto done;
    if(!xl6ExecutionTcpEnterInternalV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))goto done;
    if(!xrtOwnershipMutationBegin(&mutation))goto done;
    entered=true;
    /* Publish the queue's ownership and the source's empty state as one
     * Freeze-domain mutation. No detach/restore race with the collector. */
    result=buffer->Size?xrtNetStreamSendTake(scope.stream,buffer->Data,buffer->Size):
        xrtNetStreamSend(scope.stream,NULL,0);
    if(result==XNET_RESULT_OK){
        if(!buffer->Size)xrtFree(buffer->Data);
        *buffer=(xbuffer){0};
    }
done:
    if(entered&&!xrtOwnershipScopeEnd(&mutation))abort();
    if(scope.execution.resource)xl6ExecutionTcpLeaveInternalV1(&scope);
    if(result==XNET_RESULT_ERROR)xl6TcpFailedV1();
    return result;
}
