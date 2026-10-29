#include "xl6_x509_profiles_internal.h"
void* xl6X509CertificateKeyUsageV1(void* raw)
{
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);uint16 flags=0;if(!cert)return NULL;
    xx509result status=xrtX509KeyUsage(cert,&flags);if(status==X509_DONE)return NULL;
    if(!xl6X509OutcomeV1(status==X509_VALUE))return NULL;
    XL6_X509KeyUsageV1* result=xrtMalloc(sizeof(*result));if(result)result->flags=flags;return result;
}
