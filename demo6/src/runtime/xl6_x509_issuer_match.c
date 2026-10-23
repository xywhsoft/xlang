#include "xl6_x509_path_internal.h"
bool xl6X509IssuerMatchV1(void* raw,void* issuer)
{
    if(xrtGetError())return false;
    if(!raw || !issuer)return xl6X509FailV1(XERR_ARGUMENT,"issuer matching needs two certificates");
    xx509result result=xrtX509IssuerMatch(&((const XL6_X509CertificateV1*)raw)->view,&((const XL6_X509CertificateV1*)issuer)->view);
    if(result==X509_VALUE || result==X509_DONE)return result==X509_VALUE;
    if(result==X509_ERROR){(void)xl6X509OutcomeV1(false);return false;}
    return xl6X509FailV1(XERR_STATE,"native issuer match returned an unknown result");
}
