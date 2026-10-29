#include "xl6_tls_io_internal.h"

static xfuture* source(XL6_TlsScopeV1* scope,XL6_TlsIoKindV1 kind,void* receiver,int64_t argument,xbytesview data)
{
    if(!xl6TlsIoArgumentV1(kind,argument))return NULL;
    if(!xl6ExecutionTlsStreamEnterV1(receiver,kind!=XL6_TLS_IO_WAIT_V1,scope))return NULL;
    if(!xl6TlsIoMayWaitV1(scope->engineOwner)){xl6ExecutionTlsLeaveV1(scope);return NULL;}
    return kind==XL6_TLS_IO_RECEIVE_V1?xrtTlsStreamRecvAsync(scope->stream,(size_t)argument):
        kind==XL6_TLS_IO_WAIT_V1?xrtTlsStreamWaitAsync(scope->stream,(xtlsstreamwait)argument):
        xrtTlsStreamSendAsync(scope->stream,data.Data,data.Size);
}
xvalue* xl6TlsRecvSyncV1(void* receiver,int64_t maximum,uint64_t due,xcancel* parent)
{
    if(!xl6TlsIoPreWaitV1(due,parent)){xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsScopeV1 scope={0};xfuture* pending=source(&scope,XL6_TLS_IO_RECEIVE_V1,receiver,maximum,(xbytesview){0});
    xfutureresult native={0};bool ready=xl6TlsIoWaitResultV1(pending,due,parent,&native);xvalue* value=NULL;
    if(ready){
        if(native.State==XFUTURE_CLOSED)value=xrtValueBytes((xbytesview){0});
        else if(native.Value)value=xrtValueBytes(xrtNetBytesView(native.Value));
        else xrtSetErrorKind(XERR_STATE);
    }
    xl6TlsIoSourceDropV1(pending,!ready);if(scope.execution.resource)xl6ExecutionTlsLeaveV1(&scope);
    if(!value)xl6TlsTransportFailedV1();
    return value;
}
bool xl6TlsWaitSyncV1(void* receiver,int64_t condition,uint64_t due,xcancel* parent)
{
    if(!xl6TlsIoPreWaitV1(due,parent)){xl6TlsTransportFailedV1();return false;}
    XL6_TlsScopeV1 scope={0};xfuture* pending=source(&scope,XL6_TLS_IO_WAIT_V1,receiver,condition,(xbytesview){0});
    xfutureresult native={0};bool ready=xl6TlsIoWaitResultV1(pending,due,parent,&native);
    xl6TlsIoSourceDropV1(pending,!ready);if(scope.execution.resource)xl6ExecutionTlsLeaveV1(&scope);
    if(!ready)xl6TlsTransportFailedV1();
    return ready&&native.State==XFUTURE_RESOLVED;
}
int64_t xl6TlsSendSyncV1(void* receiver,xbytesview data,uint64_t due,xcancel* parent)
{
    if(!xl6TlsIoPreWaitV1(due,parent)){xl6TlsTransportFailedV1();return 0;}
    if(data.Size>INT64_MAX){xrtSetErrorKind(XERR_RANGE);xl6TlsTransportFailedV1();return 0;}
    XL6_TlsScopeV1 scope={0};xfuture* pending=source(&scope,XL6_TLS_IO_SEND_V1,receiver,0,data);
    xfutureresult native={0};bool ready=xl6TlsIoWaitResultV1(pending,due,parent,&native);
    if(ready&&native.State==XFUTURE_CLOSED){(void)xl6PayloadObserveOutcome(XFUTURE_CLOSED,NULL);ready=false;}
    xl6TlsIoSourceDropV1(pending,!ready);if(scope.execution.resource)xl6ExecutionTlsLeaveV1(&scope);
    if(!ready)xl6TlsTransportFailedV1();
    return ready?(int64_t)data.Size:0;
}
