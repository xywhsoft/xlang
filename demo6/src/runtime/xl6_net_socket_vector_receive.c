#include "xl6_net_vector_internal.h"
static XL6_ProductI64U64V1 receiveVector(XL6_NativeClassArrayContextV1 context,void* raw,const xvalue* array,void* information,int64_t max,bool datagram)
{
    XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};xl6_error_scope work={0};
    if(!xl6ErrorScopeBegin(&work)){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return result;}
    XL6_NetVectorInputsV1 input={0};XL6_NetSocketScopeV1 socket={0};xnetwspan spans[VECTOR_MAX];
    bool ok=max>=0&&(!datagram||information);if(!ok)xrtSetErrorKind(max<0?XERR_RANGE:XERR_ARGUMENT);
    if(ok)ok=xl6NetSocketOwnerEnterV1(raw,&socket);
    if(ok)ok=xl6NetVectorOutputCaptureV1(&input,context,array);
    if(ok&&input.count&&(uint64_t)max>(uint64_t)INT64_MAX/input.count){xrtSetErrorKind(XERR_RANGE);ok=false;}
    for(size_t i=0;ok&&i<input.count;++i)if((uint64_t)max>(uint64_t)INT64_MAX-input.buffers[i]->Size){xrtSetErrorKind(XERR_RANGE);ok=false;}
    for(size_t i=0;ok&&i<input.count;++i){xbuffer* b=input.buffers[i];ok=xrtBufferReserve(b,b->Size+(size_t)max);}
    /* A later reserve can run a host allocator. Publish no stale Data view
     * captured before all such calls have completed. */
    for(size_t i=0;ok&&i<input.count;++i){xbuffer* b=xl6BufferChecked(input.buffers[i]);
        ok=b&&b->Size==input.spans[i].Size&&(uint64_t)max<=b->Capacity-b->Size;
        if(!ok){if(b)xrtSetErrorKind(XERR_STATE);break;}
        spans[i]=(xnetwspan){max?b->Data+b->Size:NULL,(size_t)max};}
    if(ok){size_t count=0;XL6_NetDatagramInfoV1 next={0};
        result.e0=datagram?xrtNetSocketRecvMsgVec(socket.native,spans,input.count,&count,&next.remote,&next.meta):
            xrtNetSocketRecvVec(socket.native,spans,input.count,&count);result.e1=count;
        if(result.e0==XNET_RESULT_OK||result.e0==XNET_RESULT_TRUNCATED){
            size_t remaining=count;
            for(size_t i=0;i<input.count;++i){size_t used=remaining<(size_t)max?remaining:(size_t)max;
                input.buffers[i]->Size+=used;remaining-=used;}
            if(remaining)abort();
            if(datagram){next.valid=true;*(XL6_NetDatagramInfoV1*)information=next;}
        }
        ok=result.e0!=XNET_RESULT_ERROR;
    }
    xl6NetVectorCaptureFailureV1(&work,ok);
    if(socket.owner)xl6NetSocketOwnerLeaveV1(&socket);
    xl6NetVectorDiscardV1(&input,&work);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return result;
}
XL6_ProductI64U64V1 xl6NetSocketReceiveVectorIntoV1(XL6_NativeClassArrayContextV1 c,void* r,const xvalue* v,int64_t max)
{return receiveVector(c,r,v,NULL,max,false);}
XL6_ProductI64U64V1 xl6NetSocketReceiveFromVectorIntoV1(XL6_NativeClassArrayContextV1 c,void* r,const xvalue* v,void* info,int64_t max)
{return receiveVector(c,r,v,info,max,true);}
