#include "xl6_tls_session_internal.h"
static const XL6_TlsSessionRoleOpsV1 serverOperations={
    xrtTlsServerDrive,xrtTlsServerKeyUpdate,NULL,NULL,xrtTlsServerName,xrtTlsServerCookie,xrtTlsServerResumed
};
void* xl6TlsSessionServerCreateV1(void* raw)
{
    if(xrtGetError())return NULL;
    const XL6_TlsServerContextV1* source=raw;
    if(!source||!source->parameters||(!source->identity&&!source->selector.data)){
        xl6TlsFailV1(XERR_ARGUMENT,"TLS server session needs a server identity");return NULL;}
    xtlsserverconfig config;xrtTlsServerConfigInit(&config);config.Context=source->parameters;
    config.Identity=source->identity;config.RequireProtocol=source->requireProtocol;
    if(source->selector.data){config.Select=source->selector.ops->Select;config.SelectContext=source->selector.data;}
    if(source->resumeLookup.data){config.ResumeOwnedV1=source->resumeLookup.ops->Lookup;config.ResumeContext=source->resumeLookup.data;config.ResumeAgeTolerance=source->resumeAgeTolerance;}
    if(source->protocols){config.Protocols=source->protocols->views;config.ProtocolCount=source->protocols->count;}
    return xl6TlsSessionTakeCallbacksInternalV1(xrtTlsServerCreate(&config,NULL),&serverOperations,source->selector,source->resumeLookup);
}
