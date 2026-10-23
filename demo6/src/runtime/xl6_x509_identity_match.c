#include "xl6_x509_identity_internal.h"
#include "xl6_x509_profiles_internal.h"

void* xl6X509CertificateMatchHostV1(void* raw,xstrview host,int64_t size)
{
    if(xrtGetError())return NULL;
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);
    if(!cert || !xl6TextSelectRaw(&host,size))return NULL;
    xx509genname name={0};xx509result result=xrtX509MatchHost(cert,host,&name);
    if(!xl6X509IdentityResultV1(result) || result==X509_DONE)return NULL;
    /* Copy only the matched GeneralName TLV; neither a borrowed native view
     * nor the lifetime of the whole certificate escapes through this API. */
    return xl6X509GeneralNameCopyV1(&name,false);
}
