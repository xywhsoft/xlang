#include "xl6_udp_vector_internal.h"
#include <string.h>
int64_t xl6UdpVectorSendInternalV1(XL6_NetVectorCaptureV1 acquire,XL6_NativeClassArrayContextV1 context,
    void* receiver,const void* remote,const void* control,const xvalue* array,int64_t size)
{
    int64_t result=XNET_RESULT_ERROR;xl6_error_scope work={0};
    if(!xl6ErrorScopeBegin(&work)){xl6UdpFailedV1();return result;}
    XL6_NetVectorInputsV1 input={0};XL6_UdpScopeV1 scope={0};void* gathered=NULL;
    bool ok=xl6ExecutionUdpEnterInternalV1(receiver,true,&scope)&&acquire&&acquire(&input,context,array)&&xl6NetVectorSelectSizeV1(&input,size);
    if(ok){
        size_t total=0;for(size_t i=0;i<input.count;++i)total+=input.spans[i].Size;
        if(total>XNET_UDP_PAYLOAD_MAX){xrtSetErrorKind(XERR_RANGE);ok=false;}
        else if(!control)result=remote?xrtNetUdpSendVecTo(scope.udp,remote,input.spans,input.count):xrtNetUdpSendVec(scope.udp,input.spans,input.count);
        else if(!total)result=xrtNetUdpSendMsg(scope.udp,remote,control,NULL,0);
        else if((gathered=xrtMalloc(total))!=NULL){
            size_t offset=0;for(size_t i=0;i<input.count;++i){if(input.spans[i].Size)memcpy((uint8_t*)gathered+offset,input.spans[i].Data,input.spans[i].Size);offset+=input.spans[i].Size;}
            result=xrtNetUdpSendMsgTake(scope.udp,remote,control,gathered,total);if(result==XNET_RESULT_OK)gathered=NULL;
        }
        ok=ok&&result!=XNET_RESULT_ERROR;
    }
    xl6NetVectorCaptureFailureV1(&work,ok);
    if(scope.execution.resource)xl6ExecutionUdpLeaveInternalV1(&scope);
    xrtFree(gathered);xl6NetVectorDiscardV1(&input,&work);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);return result;
}
