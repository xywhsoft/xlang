#include "xl6_buffer_internal.h"
#include "xl6_net_socket_owner.h"
#include "src/api/xl_foreign_error.h"
static void failed(void){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static bool enter(void* raw,XL6_NetSocketScopeV1* scope)
{bool ok=xl6NetSocketOwnerEnterV1(raw,scope);if(!ok)failed();return ok;}
static XL6_ProductI64U64V1 sendView(void* raw,xbytesview data,int64_t size,const void* target)
{
    XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};if(!xl6BufferSelect(&data,size)){failed();return result;}
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return result;size_t count=0;
    result.e0=target?xrtNetSocketSendTo(scope.native,data.Data,data.Size,&count,target):
        xrtNetSocketSend(scope.native,data.Data,data.Size,&count);
    result.e1=count;if(result.e0==XNET_RESULT_ERROR)failed();xl6NetSocketOwnerLeaveV1(&scope);return result;
}
XL6_ProductI64U64V1 xl6NetSocketSendBytesV1(void* raw,const xvalue* input,int64_t size)
{xbytesview data;if(!xl6BufferBytesInput(input,size,&data)){failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}return sendView(raw,data,-1,NULL);}
XL6_ProductI64U64V1 xl6NetSocketSendTextV1(void* raw,xstrview input,int64_t size)
{return sendView(raw,(xbytesview){(const uint8_t*)input.Data,input.Size},size,NULL);}
XL6_ProductI64U64V1 xl6NetSocketSendBufferV1(void* raw,void* input,int64_t size)
{xbytesview data;if(!xl6BufferInput(input,size,&data)){failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}return sendView(raw,data,-1,NULL);}
XL6_ProductI64U64V1 xl6NetSocketSendToBytesV1(void* raw,const xvalue* input,const void* target,int64_t size)
{xbytesview data;if(!target||!xl6BufferBytesInput(input,size,&data)){if(!target)xrtSetErrorKind(XERR_ARGUMENT);failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}return sendView(raw,data,-1,target);}
XL6_ProductI64U64V1 xl6NetSocketSendToTextV1(void* raw,xstrview input,const void* target,int64_t size)
{if(!target){xrtSetErrorKind(XERR_ARGUMENT);failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}return sendView(raw,(xbytesview){(const uint8_t*)input.Data,input.Size},size,target);}
XL6_ProductI64U64V1 xl6NetSocketSendToBufferV1(void* raw,void* input,const void* target,int64_t size)
{xbytesview data;if(!target||!xl6BufferInput(input,size,&data)){if(!target)xrtSetErrorKind(XERR_ARGUMENT);failed();return (XL6_ProductI64U64V1){XNET_RESULT_ERROR,0};}return sendView(raw,data,-1,target);}
XL6_ProductI64U64V1 xl6NetSocketReceiveIntoV1(void* raw,void* output,int64_t maxBytes)
{
    XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};xbuffer* buffer=xl6BufferChecked(output);
    if(!buffer||maxBytes<0||(uint64_t)maxBytes>SIZE_MAX-buffer->Size||
        (uint64_t)maxBytes>(uint64_t)INT64_MAX-buffer->Size){
        if(buffer)xrtSetErrorKind(XERR_RANGE);
        failed();return result;}
    XL6_NetSocketScopeV1 scope={0};if(!enter(raw,&scope))return result;
    /* Reserve BEFORE reading: allocation failure must not consume input. The
     * caller exclusively owns this Buffer's mutable contents for this call. */
    if(!xrtBufferReserve(buffer,buffer->Size+(size_t)maxBytes))failed();
    else {size_t count=0;void* data=maxBytes?buffer->Data+buffer->Size:NULL;
        result.e0=xrtNetSocketRecv(scope.native,data,(size_t)maxBytes,&count);result.e1=count;
        if(count>(size_t)maxBytes)abort();
        if(result.e0==XNET_RESULT_OK||result.e0==XNET_RESULT_TRUNCATED)buffer->Size+=count;
        else if(result.e0==XNET_RESULT_ERROR)failed();
    }
    xl6NetSocketOwnerLeaveV1(&scope);return result;
}
