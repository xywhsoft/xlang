#include "xl6_tls_data_internal.h"
void* xl6TlsRecvOwnedSyncV1(void* receiver,int64_t maximum,uint64_t due,xcancel* parent)
{
    if(!xl6TlsIoArgumentV1(XL6_TLS_IO_RECEIVE_V1,maximum)||!xl6TlsIoPreWaitV1(due,parent)){
        xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(receiver,true,&scope)){
        xl6TlsTransportFailedV1();return NULL;}
    xfuture* source=NULL;xfutureresult result={0};void* owned=NULL;bool ready=false;
    if(xl6TlsIoMayWaitV1(scope.engineOwner)){
        source=xrtTlsStreamRecvAsync(scope.stream,(size_t)maximum);ready=xl6TlsIoWaitResultV1(source,due,parent,&result);
        if(ready){
            if(result.State==XFUTURE_CLOSED)owned=xrtNetBytesCopy(NULL,0);
            else if(result.Value)owned=xrtNetBytesRef(result.Value);
            else xrtSetErrorKind(XERR_STATE);
        }
    }
    xl6TlsIoSourceDropV1(source,!ready);xl6ExecutionTlsLeaveV1(&scope);
    if(!owned)xl6TlsTransportFailedV1();
    return owned;
}
