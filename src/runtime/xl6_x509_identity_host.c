#include "xl6_x509_identity_internal.h"
#include "xl6_x509_profiles_internal.h"

bool xl6X509CertificateMatchesHostV1(void* raw,xstrview host,int64_t size)
{
    if(xrtGetError())return false;
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);
    if(!cert || !xl6TextSelectRaw(&host,size))return false;
    xx509result result=xrtX509MatchHost(cert,host,NULL);
    return xl6X509IdentityResultV1(result) && result==X509_VALUE;
}
