#include "xl6_tls_metadata_internal.h"
#include "xl6_tls_transport_internal.h"
void* xl6TlsListenerSnapshotV1(void* receiver)
{
    XL6_TlsScopeV1 scope={0};
    if(!xl6ExecutionTlsListenerEnterV1(receiver,false,&scope)){
        xl6TlsTransportFailedV1();return NULL;
    }
    xtlslistenerstats value;
    bool ok=xrtTlsListenerStats(scope.listener,&value);
    xl6ExecutionTlsLeaveV1(&scope);
    if(!ok){xl6TlsTransportFailedV1();return NULL;}
    xtlslistenerstats* result=xrtMalloc(sizeof(*result));
    if(result)*result=value;else xl6TlsTransportFailedV1();
    return result;
}
