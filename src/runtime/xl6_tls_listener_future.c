#include "xl6_tls_class_future_internal.h"
void* xl6TlsAcceptFuturePrepareV1(XL6_NativeClassResultContextV1 context,void* receiver)
{return xl6TlsClassFuturePrepareV1(context,receiver,xl6TlsListenerHandlePolicyV1Get(),xl6ExecutionTlsPromoteAcceptV1);}
xfuture* xl6TlsAcceptSourceV1(XL6_ExecutionServicesV1 execution,void* receiver,void* raw)
{
    if(!raw||!execution.source){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTlsPrepareAcceptV1(execution.source,receiver);
    if(!carrier)return NULL;
    XL6_ExecutionResourceV1* borrowed=carrier;
    if(!xl6TlsClassFutureBindV1(raw,&carrier)){xl6ExecutionResourceReleaseV1(carrier);return NULL;}
    return xl6ExecutionTlsAcceptSourceForV1(borrowed,receiver);
}
