#include "xl6_tls_session_internal.h"
#include "xl6_buffer_internal.h"
static const XL6_TlsSessionRoleOpsV1 clientOperations={
    xrtTlsClientDrive,xrtTlsClientKeyUpdate,xrtTlsClientCertificateCount,xrtTlsClientCertificate,NULL,NULL
};
static bool name(xstrview* text,int64_t size)
{
    xbytesview view={(const uint8*)text->Data,text->Size};
    if(!xl6BufferSelect(&view,size))return false;
    *text=(xstrview){(const char*)view.Data,view.Size};return true;
}
void* xl6TlsSessionClientCreateV1(void* raw,xstrview serverName,int64_t serverSize,xstrview verifyName,int64_t verifySize)
{
    if(xrtGetError())return NULL;
    const XL6_TlsClientContextV1* source=raw;
    if(!source||!source->parameters||!source->verifier.value){
        xl6TlsFailV1(XERR_ARGUMENT,"TLS client session needs a verified client context");return NULL;}
    if(!name(&serverName,serverSize)||!name(&verifyName,verifySize))return NULL;
    if(!serverName.Size&&!verifyName.Size){xl6TlsFailV1(XERR_ARGUMENT,"TLS client session needs a verification name");return NULL;}
    xtlsclientconfig config;xrtTlsClientConfigInit(&config);config.Context=source->parameters;
    config.Verifier=source->verifier.value;config.ServerName=serverName;config.VerifyName=verifyName;
    if(source->protocols){config.Protocols=source->protocols->views;config.ProtocolCount=source->protocols->count;}
    return xl6TlsSessionTakeInternalV1(xrtTlsClientCreate(&config,NULL),&clientOperations);
}
