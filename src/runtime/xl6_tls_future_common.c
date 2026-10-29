#include "xl6_tls_io_internal.h"
xfuture* xl6TlsFutureFailV1(void* capture,xfuture* source)
{
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    if(source&&xrtFutureState(source)==XFUTURE_PENDING)(void)xrtFutureCancel(source);
    xl6FutureCallbackRuntimeV1Get()->Discard(capture);xrtFutureDestroy(source);
    xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup));return NULL;
}
xfuture* xl6TlsFuturePublishV1(XL6_RuntimeServicesV1 services,void* capture,xfuture* source)
{
    if(!source)return xl6TlsFutureFailV1(capture,NULL);
    xfuture* result=xl6FutureCallbackRuntimeV6Get()->StartExclusive(services.executor,capture,source);
    if(!result)return xl6TlsFutureFailV1(capture,source);
    xrtFutureDestroy(source);return result;
}
