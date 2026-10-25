#include "xl6_tls_dial_internal.h"
#include "xl6_net_resolver_input.h"
bool xl6TlsDialInputReadV1(xstrview host,int64_t size,int64_t port,const void* role,const void* options,
    bool proxy,XL6_TlsDialInputV1* out)
{
    if(!out||!role){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    *out=(XL6_TlsDialInputV1){0};
    if(port<=0||port>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    const XL6_TlsClientContextV1* client=role;
    if(!client->parameters||!client->verifier.value||!client->verifier.policy){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xstrview server={0},verify={0};
    if(options){const XL6_TlsDialOptionsV1* opt=options;out->config=opt->native;server=opt->serverName;verify=opt->verifyName;}
    else xl6TlsDialDefaultsV1(&out->config);
    if(!out->config.ServerNameFromHost&&!server.Size&&!verify.Size){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6TextSelect(&host,size))return false;
    xrtTlsClientConfigInit(&out->client);out->client.Context=client->parameters;out->client.Verifier=client->verifier.value;
    out->client.ServerName=server;out->client.VerifyName=verify;
    out->client.Resume=client->resume.value;
    if(client->protocols){out->client.Protocols=client->protocols->views;out->client.ProtocolCount=client->protocols->count;}
    /* Family restricts the transport endpoint. Proxy target names must never
     * be constrained/resolved locally by the proxy endpoint's family. */
    out->host=xl6NetResolverHostV1(host,proxy?XNET_FAMILY_UNSPEC:out->config.Transport.Family);
    out->port=(uint16_t)port;return out->host!=NULL;
}
