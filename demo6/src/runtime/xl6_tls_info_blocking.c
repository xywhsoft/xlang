#include "xl6_tls_metadata_internal.h"
#include "xl6_tls_io_internal.h"
void* xl6TlsStreamSnapshotV1(void* receiver,int64_t millis)
{
    xdeadline due;
    if(!xl6ConcurrentDeadline(millis,&due)||!xl6TlsIoPreWaitV1(due,NULL)){
        xl6TlsTransportFailedV1();return NULL;
    }
    XL6_TlsScopeV1 scope={0};
    if(!xl6ExecutionTlsStreamEnterV1(receiver,false,&scope)){
        xl6TlsTransportFailedV1();return NULL;
    }
    xfuture* source=NULL;xfutureresult result={0};XL6_TlsInfoV1* owned=NULL;
    bool ok=xl6TlsIoMayWaitV1(scope.engineOwner);
    if(ok){source=xl6TlsInfoSourceV1(receiver,NULL);ok=xl6TlsIoWaitResultV1(source,due,NULL,&result);}
    if(ok&&result.State==XFUTURE_RESOLVED&&result.Value){
        void *raw=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
        if(xrtValueGetHandle(*(xvalue**)result.Value,&raw,&ops,&user)&&raw&&!user&&
            ops==xl6TlsInfoHandlePolicyV1Get()->Ops)owned=xl6TlsInfoRefV1(raw);
    }
    if(!owned&&!xrtGetError())xrtSetErrorKind(XERR_STATE);
    xl6TlsIoSourceDropV1(source,!owned);xl6ExecutionTlsLeaveV1(&scope);
    if(!owned)xl6TlsTransportFailedV1();
    return owned;
}
