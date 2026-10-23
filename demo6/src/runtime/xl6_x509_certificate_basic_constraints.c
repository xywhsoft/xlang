#include "xl6_x509_profiles_internal.h"
void* xl6X509CertificateBasicConstraintsV1(void* raw)
{
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);xx509basicconstraints value;if(!cert)return NULL;
    xx509result status=xrtX509BasicConstraints(cert,&value);if(status==X509_DONE)return NULL;
    if(!xl6X509OutcomeV1(status==X509_VALUE))return NULL;
    XL6_X509BasicConstraintsV1* result=xrtMalloc(sizeof(*result));if(result)result->value=value;return result;
}
