#include "xl6_execution_tls_internal.h"
#include "xl6_tls_role_context_internal.h"
typedef struct XL6_TlsClientConnectOptionsV1 {
    const xnetaddr* remote;
    uint32_t worker;
    const XL6_TlsClientContextV1* context;
    xstrview serverName,verifyName;
    const xtlsstreamconfig* stream;
    const xnetstreamconfig* tcp;
} Client;
static xtlsstream* build(xnetengine* engine,XL6_ExecutionResourceV1* data,
    const xtlsstreamownershipv1* policy,const XL6_TlsStreamCreateOptionsV1* options)
{
    const Client* c=options->client;xtlsclientconfig tls;xrtTlsClientConfigInit(&tls);
    tls.Context=c->context->parameters;tls.Verifier=c->context->verifier.value;
    tls.ServerName=c->serverName;tls.VerifyName=c->verifyName;
    tls.Resume=c->context->resume.value;
    tls.ResumeLimit=c->context->ticketCapacity;tls.ResumeOnly=c->context->resumeRequired;
    if(c->context->protocols){tls.Protocols=c->context->protocols->views;tls.ProtocolCount=c->context->protocols->count;}
    /* XRT constructs the static-role Session synchronously: names/ALPN are
     * copied, Context/Verifier are retained by the Session. No borrowed
     * language context or name survives this call. Data is the real Resource. */
    return xrtTlsStreamConnectOwnedV1(engine,c->remote,c->worker,c->tcp,&tls,c->stream,data,policy);
}
XL6_ExecutionResourceV1* xl6ExecutionTlsConnectV1(XL6_ExecutionSourceV1* source,XL6_ExecutionResourceV1* engine,
    const xnetaddr* remote,uint32_t worker,const XL6_TlsClientContextV1* context,xstrview serverName,xstrview verifyName,
    const xtlsstreamconfig* stream,const xnetstreamconfig* tcp)
{
    if(xrtGetError())return NULL;
    if(!remote||!context||!context->parameters||!context->verifier.value||!context->verifier.policy||
        (!serverName.Data&&serverName.Size)||(!verifyName.Data&&verifyName.Size)||(!serverName.Size&&!verifyName.Size)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    const Client client={remote,worker,context,serverName,verifyName,stream,tcp};
    const XL6_TlsStreamCreateOptionsV1 tls={.engine=engine,.Build=build,.client=&client,.metadata=xl6TlsClientMetadataRoleV1Get()};
    const XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TLS_STREAM_V1,.deferRollback=true,.value.tlsStream=&tls};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionTlsStreamFamilyV1Get(),&options);
}
