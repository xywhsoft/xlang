#include "xl6_x509_crl_policy_internal.h"
void* xl6X509ValidatedCrlQueryV1(void* raw,void* certificate)
{
    if(!raw || !certificate){xl6X509FailV1(XERR_ARGUMENT,"CRL query needs validated CRL and certificate");return NULL;}
    const XL6_X509ValidatedCrlV1* self=raw;xx509revocation value={0};
    xx509result result=xrtX509CrlCheck(&self->value,&((XL6_X509CertificateV1*)certificate)->view,&value);
    if(!xl6X509OutcomeV1(result!=X509_ERROR))return NULL;
    return xl6X509RevocationCopyV1(&value,self->crl.Version,result==X509_VALUE);
}
