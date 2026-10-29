#include "xl6_tcp_internal.h"
#include "xl6_net_buffer_internal.h"
/* Unlike XRT's Worker-only chain TAKE, this ordinary send is callable from a
 * language thread, never consumes its source and performs one gather-copy. */
int64_t xl6TcpStreamSendNetBufferV1(void* receiver,void* input,int64_t size)
{
    XL6_TcpScopeV1 scope={0};XL6_NetBufferGuardV1 guard={0};
    XL6_NetBufferV1* buffer=input;xnetspan small[64],*spans=small;
    size_t bytes=0,count=0;int64_t result=XNET_RESULT_ERROR;
    if(!xl6ExecutionTcpEnterInternalV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))goto done;
    if(!xl6NetBufferEnterV1(buffer,&guard))goto done;
    if(buffer->faulted){xrtSetErrorKind(XERR_STATE);goto done;}
    if(!xl6NetBufferSelectRangeV1(buffer,0,size,&bytes))goto done;
    count=xrtNetBufSpanCount(&buffer->native);
    if(count>sizeof(small)/sizeof(small[0])){
        if(count>SIZE_MAX/sizeof(*spans)){xrtSetErrorKind(XERR_RANGE);goto done;}
        spans=xrtMalloc(count*sizeof(*spans));if(!spans)goto done;
    }
    if(xrtNetBufSpans(&buffer->native,spans,count)!=count){xrtSetErrorKind(XERR_STATE);goto done;}
    for(size_t i=0;i<count;++i){size_t selected=spans[i].Size<bytes?spans[i].Size:bytes;spans[i].Size=selected;bytes-=selected;}
    result=xrtNetStreamSendVec(scope.stream,spans,count);
done:
    if(spans!=small)xrtFree(spans);
    if(guard.owner)xl6NetBufferLeaveV1(&guard);
    if(scope.execution.resource)xl6ExecutionTcpLeaveInternalV1(&scope);
    if(result==XNET_RESULT_ERROR)xl6TcpFailedV1();
    return result;
}
