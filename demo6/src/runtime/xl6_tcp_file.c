#include "xl6_tcp_internal.h"
#include "xl6_fs_file.h"
/* Native File owns the cursor; this range operation never seeks it. XRT
 * duplicates the underlying handle before OK, so explicit close is then safe.
 * Like existing File operations, concurrent explicit close requires caller
 * serialization; no new hidden borrowing or language reference is retained. */
int64_t xl6TcpStreamSendFileV1(void* receiver,void* raw,uint64_t offset,int64_t size)
{
    if(size< -1){xrtSetErrorKind(XERR_RANGE);xl6TcpFailedV1();return XNET_RESULT_ERROR;}
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return XNET_RESULT_ERROR;
    xfile file=xlir_fs_file_get(raw);uint64_t length=0;int64_t result=XNET_RESULT_ERROR;
    if(file&&xrtFileSize(file,&length)){
        if(offset>length)xrtSetErrorKind(XERR_RANGE);
        else{
            uint64_t selected=length-offset;
            if(size>=0&&(uint64_t)size<selected)selected=(uint64_t)size;
            if(selected>SIZE_MAX)xrtSetErrorKind(XERR_RANGE);
            else result=xrtNetStreamSendFile(scope.stream,file,offset,(size_t)selected);
        }
    }
    xl6ExecutionTcpLeaveInternalV1(&scope);if(result==XNET_RESULT_ERROR)xl6TcpFailedV1();return result;
}
