#include "xl6_tls_io_internal.h"
void* xl6TlsAcceptSyncV1(XL6_ExecutionServicesV1 services,void* receiver,uint64_t due,xcancel* parent)
{
    if(!xl6TlsIoPreWaitV1(due,parent)){xl6TlsTransportFailedV1();return NULL;}
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsListenerEnterV1(receiver,true,&scope)){xl6TlsTransportFailedV1();return NULL;}
    if(!xl6TlsIoMayWaitV1(scope.engineOwner)){xl6ExecutionTlsLeaveV1(&scope);xl6TlsTransportFailedV1();return NULL;}
    xtlsstream* accepted=xrtTlsListenerAcceptWait(scope.listener,due,parent);void* result=NULL;
    if(accepted){
        result=xl6ExecutionTlsAdoptV1(services.source,scope.engineOwner,accepted);
        if(!result){xerror* error=xrtTakeError();(void)xrtTlsStreamAbort(accepted);xrtClearError();xrtSetErrorTake(error);}
        xrtTlsStreamDestroy(accepted);
    }
    xl6ExecutionTlsLeaveV1(&scope);if(!result)xl6TlsTransportFailedV1();return result;
}
