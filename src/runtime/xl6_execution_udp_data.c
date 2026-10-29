#include "xl6_execution_udp_internal.h"
typedef XL6_ExecutionResourceV1 Resource;
int xl6ExecutionUdpSendV1(Resource* r,const xnetaddr* remote,const xnetdgramcontrol* control,const void* data,size_t size)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return XNET_RESULT_ERROR;
    int result=xrtNetUdpSendMsg(scope.udp,remote,control,data,size);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
int xl6ExecutionUdpSendVecV1(Resource* r,const xnetaddr* remote,const xnetspan* spans,size_t count)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return XNET_RESULT_ERROR;
    int result=remote?xrtNetUdpSendVecTo(scope.udp,remote,spans,count):xrtNetUdpSendVec(scope.udp,spans,count);
    xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
int xl6ExecutionUdpSendTakeV1(Resource* r,const xnetaddr* remote,const xnetdgramcontrol* control,void* data,size_t size)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return XNET_RESULT_ERROR;
    int result=xrtNetUdpSendMsgTake(scope.udp,remote,control,data,size);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
int xl6ExecutionUdpSendBytesV1(Resource* r,const xnetaddr* remote,const xnetdgramcontrol* control,xnetbytes* bytes,size_t offset,size_t size)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return XNET_RESULT_ERROR;
    int result=xrtNetUdpSendMsgBytes(scope.udp,remote,control,bytes,offset,size);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
int xl6ExecutionUdpSendPacketV1(Resource* r,const xnetaddr* remote,const xnetdgramcontrol* control,xnetudppacket* packet,size_t offset,size_t size)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return XNET_RESULT_ERROR;
    int result=xrtNetUdpSendMsgPacket(scope.udp,remote,control,packet,offset,size);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
int xl6ExecutionUdpSendBytesVecV1(Resource* r,const xnetaddr* remote,const xnetdgramcontrol* control,const xnetbytesref* refs,size_t count)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return XNET_RESULT_ERROR;
    int result=xrtNetUdpSendMsgBytesVec(scope.udp,remote,control,refs,count);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
int xl6ExecutionUdpSendBatchV1(Resource* r,const xnetdgramsend* items,size_t count,size_t* accepted)
{
    if(!accepted){xrtSetErrorKind(XERR_ARGUMENT);return XNET_RESULT_ERROR;}*accepted=0;
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return XNET_RESULT_ERROR;
    int result=xrtNetUdpSendBatch(scope.udp,items,count,accepted);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
int xl6ExecutionUdpSendOwnedBatchV1(Resource* r,const xnetudpownedmessage* items,size_t count,size_t* accepted)
{
    if(!accepted){xrtSetErrorKind(XERR_ARGUMENT);return XNET_RESULT_ERROR;}*accepted=0;
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,true,&scope))return XNET_RESULT_ERROR;
    int result=xrtNetUdpSendOwnedBatch(scope.udp,items,count,accepted);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
xnetudppacket* xl6ExecutionUdpReceiveV1(Resource* r)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return NULL;
    xnetudppacket* result=xrtNetUdpReceive(scope.udp);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
xnetudperrorpacket* xl6ExecutionUdpReceiveErrorV1(Resource* r)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return NULL;
    xnetudperrorpacket* result=xrtNetUdpReceiveError(scope.udp);xl6ExecutionUdpLeaveInternalV1(&scope);return result;
}
