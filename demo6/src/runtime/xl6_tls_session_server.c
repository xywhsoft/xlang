#include "xl6_tls_session_internal.h"
void* xl6TlsSessionServerCreateV1(void* raw)
{
    if(xrtGetError())return NULL;
    const XL6_TlsServerContextV1* source=raw;
    if(!source||!source->parameters||!source->identity){
        xl6TlsFailV1(XERR_ARGUMENT,"TLS server session needs a server identity");return NULL;}
    xtlsserverconfig config;xrtTlsServerConfigInit(&config);config.Context=source->parameters;
    config.Identity=source->identity;config.RequireProtocol=source->requireProtocol;
    if(source->protocols){config.Protocols=source->protocols->views;config.ProtocolCount=source->protocols->count;}
    return xl6TlsSessionTakeInternalV1(xrtTlsServerCreate(&config,NULL),xrtTlsServerDrive);
}
