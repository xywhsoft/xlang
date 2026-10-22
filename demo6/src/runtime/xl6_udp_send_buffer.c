#include "xl6_udp_internal.h"
int64_t xl6UdpSendViewInternalV1(void*,const void*,const void*,xbytesview,int64_t);
static int64_t buffer(void* r,const void* a,const void* c,void* input,int64_t size)
{
    xbytesview view;if(!xl6BufferInput(input,size,&view)){xl6UdpFailedV1();return XNET_RESULT_ERROR;}
    return xl6UdpSendViewInternalV1(r,a,c,view,-1);
}
int64_t xl6UdpSendBufferV1(void* r,void* input,int64_t size){return buffer(r,NULL,NULL,input,size);}
int64_t xl6UdpSendToBufferV1(void* r,const void* a,void* input,int64_t size){return buffer(r,a,NULL,input,size);}
int64_t xl6UdpSendMessageBufferV1(void* r,const void* a,const void* c,void* input,int64_t size){return buffer(r,a,c,input,size);}
int64_t xl6UdpSendConnectedMessageBufferV1(void* r,const void* c,void* input,int64_t size){return buffer(r,NULL,c,input,size);}
static int64_t move(void* r,const void* a,const void* c,void* input)
{
    XL6_UdpScopeV1 scope={0};xrtownershipscope mutation={0};bool entered=false;int result=XNET_RESULT_ERROR;
    xbuffer* buffer=xl6BufferChecked(input);if(!buffer)goto done;
    if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))goto done;
    if(!xrtOwnershipMutationBegin(&mutation))goto done;
    entered=true;
    /* Empty Buffer is one real empty datagram, not an early-return no-op. */
    result=buffer->Size?xrtNetUdpSendMsgTake(scope.udp,a,c,buffer->Data,buffer->Size):xrtNetUdpSendMsg(scope.udp,a,c,NULL,0);
    if(result==XNET_RESULT_OK){if(!buffer->Size)xrtFree(buffer->Data);*buffer=(xbuffer){0};}
done:
    if(entered&&!xrtOwnershipScopeEnd(&mutation))abort();
    if(scope.execution.resource)xl6ExecutionUdpLeaveInternalV1(&scope);
    if(result==XNET_RESULT_ERROR)xl6UdpFailedV1();
    return result;
}
int64_t xl6UdpSendMoveV1(void* r,void* input){return move(r,NULL,NULL,input);}
int64_t xl6UdpSendMoveToV1(void* r,const void* a,void* input){return move(r,a,NULL,input);}
int64_t xl6UdpSendMoveMessageV1(void* r,const void* a,const void* c,void* input){return move(r,a,c,input);}
int64_t xl6UdpSendMoveConnectedMessageV1(void* r,const void* c,void* input){return move(r,NULL,c,input);}
