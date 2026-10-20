#include "xl6_buffer_internal.h"
#include "xl6_net_datagram_error_internal.h"
#include "xl6_net_socket_owner.h"
#include "src/api/xl_foreign_error.h"
static void failed(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
XL6_ProductI64U64V1 xl6NetSocketReceiveErrorIntoV1(void* raw,void* output,void* information,int64_t maxBytes)
{
    XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};
    XL6_NetDatagramErrorInfoV1* info=information;xbuffer* buffer=xl6BufferChecked(output);
    if(!buffer||!info||maxBytes<0||(uint64_t)maxBytes>SIZE_MAX-buffer->Size||
        (uint64_t)maxBytes>(uint64_t)INT64_MAX-buffer->Size){
        if(!info)xrtSetErrorKind(XERR_ARGUMENT);else if(buffer)xrtSetErrorKind(XERR_RANGE);
        failed();return result;
    }
    XL6_NetSocketScopeV1 scope={0};
    if(!xl6NetSocketOwnerEnterV1(raw,&scope)){failed();return result;}
    /* Reserve first. Native asynchronous errors are ordinary packet data:
     * Error.Kind must NOT be installed in this thread's checked-error slot. */
    if(!xrtBufferReserve(buffer,buffer->Size+(size_t)maxBytes))failed();
    else {
        XL6_NetDatagramErrorInfoV1 next={0};size_t count=0;
        result.e0=xrtNetSocketDgramRecvError(scope.native,maxBytes?buffer->Data+buffer->Size:NULL,
            (size_t)maxBytes,&count,&next.error);result.e1=count;
        if(count>(size_t)maxBytes)abort();
        if(result.e0==XNET_RESULT_OK||result.e0==XNET_RESULT_TRUNCATED){
            buffer->Size+=count;next.valid=true;*info=next;
        }else if(result.e0==XNET_RESULT_ERROR)failed();
    }
    xl6NetSocketOwnerLeaveV1(&scope);return result;
}
