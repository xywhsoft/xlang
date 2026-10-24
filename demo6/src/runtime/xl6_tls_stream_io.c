#include "xl6_tls_io_internal.h"
xfuture* xl6TlsIoSourceV1(XL6_TlsIoKindV1 kind,void* receiver,int64_t argument,xbytesview data)
{
    if(!xl6TlsIoArgumentV1(kind,argument))return NULL;
    XL6_TlsScopeV1 scope={0};
    if(!xl6ExecutionTlsStreamEnterV1(receiver,kind!=XL6_TLS_IO_WAIT_V1,&scope))return NULL;
    xfuture* source=kind==XL6_TLS_IO_RECEIVE_V1?xrtTlsStreamRecvAsync(scope.stream,(size_t)argument):
        kind==XL6_TLS_IO_WAIT_V1?xrtTlsStreamWaitAsync(scope.stream,(xtlsstreamwait)argument):
        xrtTlsStreamSendAsync(scope.stream,data.Data,data.Size);
    xl6ExecutionTlsLeaveV1(&scope);return source;
}
