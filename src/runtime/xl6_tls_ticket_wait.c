#include "xl6_tls_ticket_worker.h"
static bool begin(void* receiver,bool mutates,XL6_TlsScopeV1* scope)
{
    if(!xl6ExecutionTlsStreamEnterV1(receiver,mutates,scope))return false;
    if(xl6TlsIoMayWaitV1(scope->engineOwner))return true;
    xl6ExecutionTlsLeaveV1(scope);return false;
}
void xl6TlsTicketClassSyncV1(XL6_NativeClassResultContextV1 context,void* receiver,
    const XL6_TlsTicketCommandV1* command,xbytesview input,uint32_t lifetime,xvalue** output)
{
    if(!output||*output){xrtSetErrorKind(XERR_ARGUMENT);goto failed;}
    XL6_TlsScopeV1 scope={0};if(!begin(receiver,true,&scope))goto failed;
    /* Control commits must not be disguised as an effect-free timeout. There
     * is no user deadline here; use Async to avoid blocking the calling thread.
     * Cooperative cancellation is checked before the native linearization. */
    xfuture* source=xl6TlsTicketSubmitV1(&context,receiver,command,input,lifetime);xfutureresult result={0};
    bool ok=xl6TlsIoWaitResultV1(source,XRT_DEADLINE_NEVER,NULL,&result);
    if(ok&&(result.State!=XFUTURE_RESOLVED||!result.Value||!*(xvalue**)result.Value)){
        xrtSetErrorKind(XERR_STATE);ok=false;
    }
    if(ok){*output=xrtValueRetain(*(xvalue**)result.Value);ok=*output!=NULL;}
    xl6TlsIoSourceDropV1(source,!ok);xl6ExecutionTlsLeaveV1(&scope);
    if(ok)return;
failed:
    xl6TlsTransportFailedV1();
}
uint64_t xl6TlsTicketScalarSyncV1(void* receiver,const XL6_TlsTicketCommandV1* command,int64_t millis)
{
    xdeadline due;uint64_t value=0;
    if(!xl6ConcurrentDeadline(millis,&due)||!xl6TlsIoPreWaitV1(due,NULL))goto failed;
    XL6_TlsScopeV1 scope={0};if(!begin(receiver,false,&scope))goto failed;
    xfuture* source=xl6TlsTicketSubmitV1(NULL,receiver,command,(xbytesview){0},0);xfutureresult result={0};
    bool ok=xl6TlsIoWaitResultV1(source,due,NULL,&result);
    if(ok&&(result.State!=XFUTURE_RESOLVED||!result.Value)){xrtSetErrorKind(XERR_STATE);ok=false;}
    if(ok)memcpy(&value,result.Value,sizeof(value));
    xl6TlsIoSourceDropV1(source,!ok);xl6ExecutionTlsLeaveV1(&scope);
    if(ok)return value;
failed:
    xl6TlsTransportFailedV1();return 0;
}
