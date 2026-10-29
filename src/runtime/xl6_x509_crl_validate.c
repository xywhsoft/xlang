#include "xl6_x509_crl_policy_internal.h"

void* xl6X509CrlValidateV1(void* crl,void* issuer,int64_t time,int64_t skew,int64_t flags)
{
    xx509crlconfig config;
    return xl6X509CrlPolicyConfigV1(time,skew,(uint64_t)flags,NULL,NULL,&config)?
        xl6X509ValidatedCrlCreateV1(crl,issuer,&config):NULL;
}
