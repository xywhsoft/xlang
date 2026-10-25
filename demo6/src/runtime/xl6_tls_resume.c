#include "xl6_tls_internal.h"
#include <limits.h>
#include <string.h>

void* xl6TlsResumeFromPartsV1(int64_t version,int64_t cipher,const xvalue* ticket,
    const xvalue* secret,int64_t issuedAt,int64_t lifetime,uint64_t ageAdd,
    xstrview serverName,const xvalue* protocol,const xvalue* peerIdentity,uint64_t maxEarlyData)
{
    if(xrtGetError())return NULL;
    /* Check before narrowing; XRT validates supported suites, full credential
     * lengths and lifetime. Credential inputs must never be prefix-truncated. */
    if(version<0||version>UINT16_MAX||cipher<0||cipher>UINT16_MAX||
        lifetime<0||(uint64_t)lifetime>UINT32_MAX||ageAdd>UINT32_MAX||maxEarlyData>UINT32_MAX){
        xl6TlsFailV1(XERR_RANGE,"TLS resume numeric field exceeds its native range");return NULL;
    }
    xtlsresumeconfig config={0};
    config.Version=(xtlsversion)version;config.Cipher=(xtlscipher)cipher;
    config.IssuedAt=issuedAt;config.Lifetime=(uint32)lifetime;
    config.AgeAdd=(uint32)ageAdd;config.MaxEarlyData=(uint32)maxEarlyData;config.ServerName=serverName;
    if(!xrtValueGetBytes(ticket,&config.Ticket)||!xrtValueGetBytes(secret,&config.Secret)||
        !xrtValueGetBytes(protocol,&config.Protocol)||!xrtValueGetBytes(peerIdentity,&config.PeerIdentity))return NULL;
    return xrtTlsResumeCreate(&config);
}
void xl6TlsResumeDestroyV1(void* raw){xrtTlsResumeRelease(raw);}
static bool info(const void* raw,xtlsresumeinfo* result)
{return !xrtGetError()&&xrtTlsResumeInfo(raw,result);}

#define XL6_TLS_RESUME_NUMBER(Name,Field,Type) \
Type xl6TlsResume##Name##V1(const void* raw) \
{xtlsresumeinfo value={0};return info(raw,&value)?(Type)value.Field:0;}
XL6_TLS_RESUME_NUMBER(Version,Version,int64_t)
XL6_TLS_RESUME_NUMBER(Cipher,Cipher,int64_t)
XL6_TLS_RESUME_NUMBER(Lifetime,Lifetime,int64_t)
XL6_TLS_RESUME_NUMBER(AgeAdd,AgeAdd,uint64_t)
XL6_TLS_RESUME_NUMBER(MaxEarlyData,MaxEarlyData,uint64_t)
XL6_TLS_RESUME_NUMBER(IssuedAt,IssuedAt,int64_t)
XL6_TLS_RESUME_NUMBER(ExpiresAt,ExpiresAt,int64_t)
#undef XL6_TLS_RESUME_NUMBER
#define XL6_TLS_RESUME_BYTES(Name,Field) \
xvalue* xl6TlsResume##Name##V1(const void* raw) \
{xtlsresumeinfo value={0};return info(raw,&value)?xrtValueBytes(value.Field):NULL;}
XL6_TLS_RESUME_BYTES(Ticket,Ticket)
XL6_TLS_RESUME_BYTES(ExportSecret,Secret)
XL6_TLS_RESUME_BYTES(Protocol,Protocol)
XL6_TLS_RESUME_BYTES(PeerIdentity,PeerIdentity)
#undef XL6_TLS_RESUME_BYTES
xstrview xl6TlsResumeServerNameV1(const void* raw)
{
    xtlsresumeinfo value={0};if(!info(raw,&value))return (xstrview){0};
    char* text=xrtMalloc(value.ServerName.Size+1);if(!text)return (xstrview){0};
    if(value.ServerName.Size)memcpy(text,value.ServerName.Data,value.ServerName.Size);
    text[value.ServerName.Size]=0;return (xstrview){text,value.ServerName.Size};
}
bool xl6TlsResumeValidAtV1(const void* raw,int64_t at)
{return !xrtGetError()&&xrtTlsResumeValidAt(raw,at);}
uint64_t xl6TlsResumeTicketAgeV1(const void* raw,int64_t at)
{
    if(xrtGetError())return 0;
    uint32 age=0;
    if(!xrtTlsResumeTicketAge(raw,at,&age)){
        if(!xrtGetError())xl6TlsFailV1(XERR_STATE,"TLS resume has no ticket age outside its validity interval");
        return 0;
    }
    return age;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsResumeHandlePolicyV1Get,xl6TlsResumeDestroyV1)
