#include "xl6_tls_metadata_internal.h"
#include "xl6_tls_transport_internal.h"

static bool add(size_t* size,size_t extra)
{
    if(extra>SIZE_MAX-*size){xrtSetErrorKind(XERR_RANGE);return false;}
    *size+=extra;return true;
}
static xstrview text(const char* value)
{return (xstrview){value,value?strlen(value):0};}
static xbytesview copyBytes(uint8_t** next,xbytesview view)
{
    xbytesview result={*next,view.Size};
    if(view.Size)memcpy(*next,view.Data,view.Size);
    *next+=view.Size;return result;
}
static xstrview copyText(uint8_t** next,xstrview view)
{
    xbytesview result=copyBytes(next,(xbytesview){(const uint8_t*)view.Data,view.Size});
    return (xstrview){(const char*)result.Data,result.Size};
}

/* Invoked synchronously on the actual stream Worker. No TLS drive or callback
 * can interleave the sizing and copying passes on that Worker. The caller's
 * execution scope pins native storage through publication/rollback. */
XL6_TlsInfoV1* xl6TlsInfoBuildV1(xtlsstream* stream,const XL6_TlsMetadataRoleV1* role)
{
    xtlssession* session=xrtTlsStreamSession(stream);
    xnetstream* transport=xrtTlsStreamTransport(stream);
    xnetworker* worker=transport?xrtNetStreamWorker(transport):NULL;
    if(!session||!worker||xrtNetEngineCurrent(xrtNetWorkerEngine(worker))!=worker){
        xrtSetErrorKind(XERR_STATE);return NULL;
    }
    XL6_TlsInfoV1 fields={0};
    fields.references=1;
    fields.role=xrtTlsSessionRole(session);
    if(!role||role->role!=fields.role){xrtSetErrorKind(XERR_UNSUPPORTED);return NULL;}
    fields.state=xrtTlsStreamState(stream);
    fields.sessionState=xrtTlsSessionState(session);
    fields.ready=fields.sessionState==XTLS_STATE_READY;
    fields.version=xrtTlsSessionVersion(session);
    fields.cipher=xrtTlsSessionCipher(session);
    fields.hasSelection=fields.version!=0&&fields.cipher!=0;
    fields.available=xrtTlsStreamAvailable(stream);
    fields.pending=xrtTlsStreamPending(stream);
    fields.asyncBytes=xrtTlsStreamAsyncBytes(stream);
    fields.asyncCount=xrtTlsStreamAsyncCount(stream);
    if(role->Resumed)fields.resumed=role->Resumed(session);
    if(role->ServerName)(void)role->ServerName(session,&fields.serverName);
    if(role->ServerCookie)fields.hasServerCookie=role->ServerCookie(session,&fields.serverCookie);
    (void)xrtTlsSessionProtocol(session,&fields.protocol);
    xtlsalertlevel level=0;xtlsalert alert=0;
    fields.hasPeerAlert=xrtTlsSessionPeerAlert(session,&level,&alert);
    fields.peerAlertLevel=level;fields.peerAlertCode=alert;
    const xerror* error=xrtTlsStreamError(stream);
    fields.hasError=error!=NULL;
    if(error){
        fields.errorKind=xrtErrorKind(error);fields.errorCode=xrtErrorCode(error);
        fields.errorDomain=text(xrtErrorDomain(error));
        fields.errorOperation=text(xrtErrorOperation(error));
        fields.errorMessage=text(xrtErrorMessage(error));
    }
    /* Parsed certificates are not authenticated before Finished. Never turn
     * an incomplete/failed handshake's provisional chain into a peer result. */
    bool authenticated=fields.sessionState==XTLS_STATE_READY||
        fields.sessionState==XTLS_STATE_CLOSING||fields.sessionState==XTLS_STATE_CLOSED;
    if(authenticated&&role->CertificateCount)
        fields.certificateCount=role->CertificateCount(session);
    if(fields.certificateCount&&!role->CertificateAt){xrtSetErrorKind(XERR_STATE);return NULL;}
    if(xrtGetError())return NULL;
    if(fields.certificateCount>INT64_MAX||fields.certificateCount>(SIZE_MAX-sizeof(fields))/sizeof(xbytesview)){
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    size_t size=sizeof(fields)+fields.certificateCount*sizeof(xbytesview);
    if(!add(&size,fields.protocol.Size)||!add(&size,fields.serverName.Size)||
        !add(&size,fields.errorDomain.Size)||!add(&size,fields.errorOperation.Size)||
        !add(&size,fields.errorMessage.Size))return NULL;
    /* Every returned text getter reserves its own terminating byte. */
    if(fields.errorDomain.Size==SIZE_MAX||fields.errorOperation.Size==SIZE_MAX||
        fields.errorMessage.Size==SIZE_MAX){xrtSetErrorKind(XERR_RANGE);return NULL;}
    for(size_t i=0;i<fields.certificateCount;i++){
        const xx509cert* certificate=role->CertificateAt(session,i);
        if(!certificate||!certificate->Raw.Data||!certificate->Raw.Size){
            if(!xrtGetError())xrtSetErrorKind(XERR_STATE);
            return NULL;
        }
        if(!add(&size,certificate->Raw.Size))return NULL;
    }
    XL6_TlsInfoV1* result=xrtMalloc(size);if(!result)return NULL;
    *result=fields;
    uint8_t* next=(uint8_t*)(result->certificates+fields.certificateCount);
    result->protocol=copyBytes(&next,fields.protocol);
    result->serverName=copyBytes(&next,fields.serverName);
    result->errorDomain=copyText(&next,fields.errorDomain);
    result->errorOperation=copyText(&next,fields.errorOperation);
    result->errorMessage=copyText(&next,fields.errorMessage);
    for(size_t i=0;i<fields.certificateCount;i++){
        const xx509cert* certificate=role->CertificateAt(session,i);
        if(!certificate){
            xrtFree(result);if(!xrtGetError())xrtSetErrorKind(XERR_STATE);return NULL;
        }
        result->certificates[i]=copyBytes(&next,certificate->Raw);
    }
    return result;
}
