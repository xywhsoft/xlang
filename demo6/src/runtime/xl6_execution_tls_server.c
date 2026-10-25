#include "xl6_execution_tls_internal.h"
#include "xl6_tls_role_context_internal.h"
XL6_ExecutionResourceV1* xl6ExecutionTlsListenV1(XL6_ExecutionSourceV1* source,XL6_ExecutionResourceV1* engine,
    const XL6_TlsServerContextV1* context,const xtlslistenerconfig* options)
{
    if(xrtGetError())return NULL;
    if(!context||!context->parameters||(!context->identity&&!context->selector.data)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xtlslistenerconfig config;if(options)config=*options;else xrtTlsListenerConfigInit(&config);
    /* Only the explicit immutable role context supplies authentication.
     * No caller's raw selector/resume callback is smuggled through options. */
    xrtTlsServerConfigInit(&config.Tls);config.Tls.Context=context->parameters;config.Tls.Identity=context->identity;
    if(context->selector.data){config.Tls.Select=context->selector.ops->Select;config.Tls.SelectContext=context->selector.data;}
    if(context->resumeLookup.data){config.Tls.ResumeOwnedV1=context->resumeLookup.ops->Lookup;config.Tls.ResumeContext=context->resumeLookup.data;config.Tls.ResumeAgeTolerance=context->resumeAgeTolerance;}
    config.Tls.RequireProtocol=context->requireProtocol;
    if(context->protocols){config.Tls.Protocols=context->protocols->views;config.Tls.ProtocolCount=context->protocols->count;}
    const XL6_TlsListenerCreateOptionsV1 tls={.engine=engine,.config=&config,.metadata=xl6TlsServerMetadataRoleV1Get(),.selector=context->selector,.resumeLookup=context->resumeLookup};
    const XL6_ExecutionCreateOptionsV1 create={.kind=XL6_EXECUTION_TLS_LISTENER_V1,.deferRollback=true,.value.tlsListener=&tls};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionTlsListenerFamilyV1Get(),&create);
}
