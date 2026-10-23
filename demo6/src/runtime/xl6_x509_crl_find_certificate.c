#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509CrlFindCertificateV1(void* raw,void* certificate)
{
    const xx509crl* crl=xl6X509ProfileCrlV1(raw);const xx509cert* cert=xl6X509ProfileCertificateV1(certificate);xx509crlentry entry;
    if(!crl || !cert)return NULL;
    xx509result status=xrtX509CrlRevokes(crl,cert,&entry);if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509CrlEntryCopyV1(entry.Raw,crl->Version):NULL;
}
