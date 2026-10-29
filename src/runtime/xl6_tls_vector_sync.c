#include "xl6_tls_data_internal.h"
int64_t xl6TlsVectorSyncV1(XL6_NetVectorCaptureV1 acquire,XL6_NativeClassArrayContextV1 context,
    void* receiver,const xvalue* array,int64_t size,uint64_t due,xcancel* parent)
{
    if(!xl6TlsIoPreWaitV1(due,parent)){xl6TlsTransportFailedV1();return 0;}
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(receiver,true,&scope)){
        xl6TlsTransportFailedV1();return 0;}
    xfuture* source=NULL;size_t count=0;bool ready=false;xfutureresult result={0};
    if(xl6TlsIoMayWaitV1(scope.engineOwner)){
        source=xl6TlsVectorSourceV1(acquire,context,receiver,array,size,&count);
        ready=xl6TlsIoWaitResultV1(source,due,parent,&result);
        if(ready&&result.State==XFUTURE_CLOSED){(void)xl6PayloadObserveOutcome(XFUTURE_CLOSED,NULL);ready=false;}
    }
    xl6TlsIoSourceDropV1(source,!ready);xl6ExecutionTlsLeaveV1(&scope);
    if(!ready)xl6TlsTransportFailedV1();
    return ready?(int64_t)count:0;
}
