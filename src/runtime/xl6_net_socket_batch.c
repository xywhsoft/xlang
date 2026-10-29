#include "xl6_net_datagram_batch_internal.h"
#include "xl6_native_class_borrow.h"
#include "xl6_net_socket_owner.h"
#include "src/api/xl_foreign_error.h"
enum {BATCH_MAX=64};
static void capture(xl6_error_scope* scope,bool ok)
{if(!ok||xrtGetError()||scope->failed)xl6ErrorScopeCaptureFailure(scope);}
static XL6_ProductI64U64V1 invoke(XL6_NativeClassArrayContextV1 context,void* raw,const xvalue* input,
    void* progress,int64_t maxBytes,int64_t start,int64_t requested,bool receive)
{
    XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};XL6_NetBatchProgressV1* report=progress;
    xl6_error_scope work={0};if(!xl6ErrorScopeBegin(&work)){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return result;}
    if(report)xl6NetBatchProgressClearV1(report);
    XL6_NativeClassArrayPinV1 pin={0};XL6_NetSocketScopeV1 socket={0};
    xvalue* held[BATCH_MAX]={0};XL6_NetDatagramPacketV1* packets[BATCH_MAX];xbuffer* buffers[BATCH_MAX];
    xnetdgramsend sends[BATCH_MAX];xnetdgramrecv reads[BATCH_MAX];size_t count=0,retained=0;
    bool ok=report&&start>=0&&requested>=-1&&(!receive||(maxBytes>=0&&(uint64_t)maxBytes<=SIZE_MAX));
    if(!ok)xrtSetErrorKind(!report?XERR_ARGUMENT:XERR_RANGE);
    if(ok)ok=xl6NetSocketOwnerEnterV1(raw,&socket);
    if(ok)ok=xl6NativeClassArrayBeginV1(&pin,context,input,xl6NetDatagramPacketHandlePolicyV1Get());
    if(ok){size_t total=xrtTypedArrayCount(xl6SequenceStorageV1(pin.owner));
        if((uint64_t)start>total){xrtSetErrorKind(XERR_RANGE);ok=false;}
        else {count=total-(size_t)start;if(requested>=0&&(uint64_t)requested<count)count=(size_t)requested;
            if(count>BATCH_MAX){xrtSetErrorKind(XERR_RANGE);ok=false;}}}
    for(size_t i=0;ok&&i<count;++i){
        void* value=NULL;held[i]=xl6NativeClassArrayRetainAtV1(&pin,(size_t)start+i,&value);
        if(!held[i]){ok=false;break;}retained=i+1;packets[i]=value;
        buffers[i]=xl6NetDatagramPacketBufferV1(packets[i]);if(!buffers[i]){ok=false;break;}
        for(size_t j=0;receive&&j<i;++j)if(buffers[i]==buffers[j]){xrtSetErrorKind(XERR_VALUE);ok=false;break;}
    }
    /* All fallible capacity changes occur before consuming any input packet. */
    for(size_t i=0;ok&&receive&&i<count;++i)ok=xrtBufferReserve(buffers[i],(size_t)maxBytes);
    for(size_t i=0;ok&&receive&&i<count;++i){xbuffer* buffer=xl6BufferChecked(buffers[i]);
        ok=buffer&&(uint64_t)maxBytes<=buffer->Capacity;
        if(!ok){if(buffer)xrtSetErrorKind(XERR_STATE);break;}
        reads[i]=(xnetdgramrecv){.Data=maxBytes?buffer->Data:NULL,.Capacity=(size_t)maxBytes};}
    for(size_t i=0;ok&&!receive&&i<count;++i)sends[i]=(xnetdgramsend){
        packets[i]->hasTarget?&packets[i]->target:NULL,buffers[i]->Data,buffers[i]->Size};
    if(ok){size_t completed=0;
        result.e0=receive?xrtNetSocketRecvBatch(socket.native,reads,count,&completed):xrtNetSocketSendBatch(socket.native,sends,count,&completed);
        if(completed>count)abort();
        result.e1=completed;
        *report=(XL6_NetBatchProgressV1){completed,(uint64_t)start,result.e0,true};
        for(size_t i=0;receive&&i<completed;++i){
            if(reads[i].Size>(size_t)maxBytes)abort();
            buffers[i]->Size=reads[i].Size;
            packets[i]->hasResult=true;packets[i]->status=reads[i].Result;
            packets[i]->info=(XL6_NetDatagramInfoV1){reads[i].Remote,reads[i].Meta,
                reads[i].Result==XNET_RESULT_OK||reads[i].Result==XNET_RESULT_TRUNCATED};
        }
        ok=result.e0!=XNET_RESULT_ERROR;
    }
    capture(&work,ok);if(socket.owner)xl6NetSocketOwnerLeaveV1(&socket);
    for(size_t i=retained;i>0;--i){xrtValueRelease(held[i-1]);capture(&work,true);}
    xl6NativeClassArrayEndV1(&pin);capture(&work,true);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return result;
}
XL6_ProductI64U64V1 xl6NetSocketSendBatchV1(XL6_NativeClassArrayContextV1 c,void* s,const xvalue* a,void* p,int64_t start,int64_t count)
{return invoke(c,s,a,p,0,start,count,false);}
XL6_ProductI64U64V1 xl6NetSocketReceiveBatchV1(XL6_NativeClassArrayContextV1 c,void* s,const xvalue* a,void* p,int64_t max,int64_t start,int64_t count)
{return invoke(c,s,a,p,max,start,count,true);}
