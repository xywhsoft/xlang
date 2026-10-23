#include "xl6_x509_profiles_internal.h"
xvalue* xl6X509CertificateSubjectKeyIdV1(void* raw)
{
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);xbytesview value;if(!cert)return NULL;
    /* bytes? uses an owned Value/null-tag carrier, not the class factory's
     * raw-pointer sentinel. C NULL remains failure, never ordinary absence. */
    xx509result status=xrtX509SubjectKeyId(cert,&value);
    if(status==X509_DONE)return xrtValueRetain(xrtValueNull());
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509BytesV1(value):NULL;
}
