#include "xl6_tls_dial_internal.h"
void* xl6TlsDialSyncV1(XL6_ExecutionServicesV1 execution,void* engine,void* resolver,
    const XL6_TlsDialInputV1* input,const xnetproxy* proxy,XL6_TlsDialSourceV1 submit,uint64_t due,xcancel* parent)
{
    if(!submit||!input){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    if(!xl6TlsIoPreWaitV1(due,parent)||!xl6TlsIoMayWaitV1(engine)){xl6TlsTransportFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTlsPrepareDialV1(execution.source,engine,resolver);
    if(!carrier){xl6TlsTransportFailedV1();return NULL;}
    xfuture* source=submit(carrier,input,proxy);xfutureresult result={0};
    bool ok=xl6TlsIoWaitResultV1(source,due,parent,&result);
    if(ok&&result.State!=XFUTURE_RESOLVED){xrtSetErrorKind(XERR_STATE);ok=false;}
    if(ok)ok=xl6ExecutionTlsPromoteDialV1(carrier);
    xl6TlsIoSourceDropV1(source,!ok);
    if(!ok){xl6ExecutionResourceReleaseV1(carrier);xl6TlsTransportFailedV1();return NULL;}
    return carrier;
}
