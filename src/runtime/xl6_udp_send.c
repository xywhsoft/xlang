#include "xl6_udp_internal.h"
int64_t xl6UdpSendViewInternalV1(void* receiver,const void* remote,const void* control,xbytesview view,int64_t size)
{
    if(!xl6BufferSelect(&view,size)){xl6UdpFailedV1();return XNET_RESULT_ERROR;}
    int result=xl6ExecutionUdpSendV1(receiver,remote,control,view.Data,view.Size);
    if(result==XNET_RESULT_ERROR)xl6UdpFailedV1();
    return result;
}
static int64_t sendText(void* r,const void* a,const void* c,xstrview input,int64_t size)
{return xl6UdpSendViewInternalV1(r,a,c,(xbytesview){(const uint8_t*)input.Data,input.Size},size);}
static int64_t sendBytes(void* r,const void* a,const void* c,const xvalue* input,int64_t size)
{
    xbytesview view;if(!xl6BufferBytesInput(input,size,&view)){xl6UdpFailedV1();return XNET_RESULT_ERROR;}
    return xl6UdpSendViewInternalV1(r,a,c,view,-1);
}
#define SEND(Name,Type,Call) \
int64_t xl6UdpSend##Name##V1(void* r,Type input,int64_t size){return Call(r,NULL,NULL,input,size);} \
int64_t xl6UdpSendTo##Name##V1(void* r,const void* a,Type input,int64_t size){return Call(r,a,NULL,input,size);} \
int64_t xl6UdpSendMessage##Name##V1(void* r,const void* a,const void* c,Type input,int64_t size){return Call(r,a,c,input,size);} \
int64_t xl6UdpSendConnectedMessage##Name##V1(void* r,const void* c,Type input,int64_t size){return Call(r,NULL,c,input,size);}
SEND(Text,xstrview,sendText)
SEND(Bytes,const xvalue*,sendBytes)
#undef SEND
