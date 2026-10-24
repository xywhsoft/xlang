#include "xl6_tls_data_internal.h"
static bool perform(void* receiver,int64_t argument,unsigned operation,int64_t millis,xvalue** bytes,int64_t* count)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)||!xl6TlsIoPreWaitV1(due,NULL))return false;
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(receiver,operation==XL6_TLS_WORKER_CONSUME_V1,&scope))return false;
    xfuture* source=NULL;xfutureresult result={0};bool ok=xl6TlsIoMayWaitV1(scope.engineOwner);
    if(ok){source=xl6TlsWorkerSourceV1(receiver,argument,operation);ok=xl6TlsIoWaitResultV1(source,due,NULL,&result);}
    if(ok&&(!result.Value||result.State!=XFUTURE_RESOLVED)){xrtSetErrorKind(XERR_STATE);ok=false;}
    if(ok){
        if(bytes){*bytes=xrtValueRetain(*(xvalue**)result.Value);ok=*bytes!=NULL;}
        else *count=*(int64_t*)result.Value;
    }
    xl6TlsIoSourceDropV1(source,!ok);xl6ExecutionTlsLeaveV1(&scope);return ok;
}
xvalue* xl6TlsStreamPeekV1(void* receiver,int64_t maximum,int64_t millis)
{xvalue* result=NULL;if(!perform(receiver,maximum,XL6_TLS_WORKER_PEEK_V1,millis,&result,NULL))xl6TlsTransportFailedV1();return result;}
int64_t xl6TlsStreamConsumeV1(void* receiver,int64_t count,int64_t millis)
{int64_t result=0;if(!perform(receiver,count,XL6_TLS_WORKER_CONSUME_V1,millis,NULL,&result))xl6TlsTransportFailedV1();return result;}
int64_t xl6TlsStreamSendBoundV1(void* receiver,int64_t count,int64_t millis)
{int64_t result=0;if(!perform(receiver,count,XL6_TLS_WORKER_SEND_BOUND_V1,millis,NULL,&result))xl6TlsTransportFailedV1();return result;}
xvalue* xl6TlsStreamProtocolV1(void* receiver,int64_t millis)
{xvalue* result=NULL;if(!perform(receiver,0,XL6_TLS_WORKER_PROTOCOL_V1,millis,&result,NULL))xl6TlsTransportFailedV1();return result;}
