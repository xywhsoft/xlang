#include "xl6_buffer_internal.h"
#include "xl6_net_datagram_internal.h"
#include "xl6_net_socket_owner.h"
#include "src/api/xl_foreign_error.h"
static void failed(void)
{ xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); }
XL6_ProductI64U64V1 xl6NetSocketReceiveFromIntoV1(void* raw,void* output,void* information,int64_t maxBytes)
{
    XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};
    XL6_NetDatagramInfoV1* info=information;
    xbuffer* buffer=xl6BufferChecked(output);
    if(!buffer||!info||maxBytes<0||(uint64_t)maxBytes>SIZE_MAX-buffer->Size||
        (uint64_t)maxBytes>(uint64_t)INT64_MAX-buffer->Size){
        if(!info)xrtSetErrorKind(XERR_ARGUMENT);else if(buffer)xrtSetErrorKind(XERR_RANGE);
        failed();return result;
    }
    XL6_NetSocketScopeV1 scope={0};
    if(!xl6NetSocketOwnerEnterV1(raw,&scope)){failed();return result;}
    /* All fallible allocation is BEFORE receive. On AGAIN/error both Info and
     * the Buffer's logical content remain unchanged (capacity may grow). */
    if(!xrtBufferReserve(buffer,buffer->Size+(size_t)maxBytes))failed();
    else {
        XL6_NetDatagramInfoV1 next={0};size_t count=0;
        result.e0=xrtNetSocketRecvMsg(scope.native,maxBytes?buffer->Data+buffer->Size:NULL,
            (size_t)maxBytes,&count,&next.remote,&next.meta);
        result.e1=count;
        if(count>(size_t)maxBytes)abort();
        if(result.e0==XNET_RESULT_OK||result.e0==XNET_RESULT_TRUNCATED){
            buffer->Size+=count;next.valid=true;*info=next;
        } else if(result.e0==XNET_RESULT_ERROR)failed();
    }
    xl6NetSocketOwnerLeaveV1(&scope);return result;
}
static XL6_ProductI64U64V1 sendView(void* raw,xbytesview data,const void* target,const void* control,int64_t size,bool connected)
{
    XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};
    if((!target&&!connected)||!control||!xl6BufferSelect(&data,size)){
        if((!target&&!connected)||!control)xrtSetErrorKind(XERR_ARGUMENT);
        failed();return result;
    }
    XL6_NetSocketScopeV1 scope={0};
    if(!xl6NetSocketOwnerEnterV1(raw,&scope)){failed();return result;}
    size_t count=0;
    result.e0=xrtNetSocketSendMsg(scope.native,data.Data,data.Size,&count,target,control);result.e1=count;
    if(result.e0==XNET_RESULT_ERROR)failed();
    xl6NetSocketOwnerLeaveV1(&scope);return result;
}
XL6_ProductI64U64V1 xl6NetSocketSendMessageTextV1(void* r,xstrview input,const void* a,const void* c,int64_t size)
{ return sendView(r,(xbytesview){(const uint8_t*)input.Data,input.Size},a,c,size,false); }
XL6_ProductI64U64V1 xl6NetSocketSendMessageBytesV1(void* r,const xvalue* input,const void* a,const void* c,int64_t size)
{
    xbytesview data;if(!xl6BufferBytesInput(input,size,&data)){failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}
    return sendView(r,data,a,c,-1,false);
}
XL6_ProductI64U64V1 xl6NetSocketSendMessageBufferV1(void* r,void* input,const void* a,const void* c,int64_t size)
{
    xbytesview data;if(!xl6BufferInput(input,size,&data)){failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}
    return sendView(r,data,a,c,-1,false);
}
/* Only these explicit entry points may omit a destination. The ordinary
 * forms still reject a NULL target, never silently changing their meaning. */
XL6_ProductI64U64V1 xl6NetSocketSendConnectedMessageTextV1(void* r,xstrview input,const void* c,int64_t size)
{ return sendView(r,(xbytesview){(const uint8_t*)input.Data,input.Size},NULL,c,size,true); }
XL6_ProductI64U64V1 xl6NetSocketSendConnectedMessageBytesV1(void* r,const xvalue* input,const void* c,int64_t size)
{
    xbytesview data;if(!xl6BufferBytesInput(input,size,&data)){failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}
    return sendView(r,data,NULL,c,-1,true);
}
XL6_ProductI64U64V1 xl6NetSocketSendConnectedMessageBufferV1(void* r,void* input,const void* c,int64_t size)
{
    xbytesview data;if(!xl6BufferInput(input,size,&data)){failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}
    return sendView(r,data,NULL,c,-1,true);
}
