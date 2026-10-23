#include "xl6_x509_profiles_internal.h"
void* xl6X509CertificateExtendedKeyUsageV1(void* raw,int64_t limit)
{
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);xx509oidcursor cursor;
    if(!cert || !xl6X509LimitV1(0,limit,"invalid extended key usage budget"))return NULL;
    xx509result status=xrtX509ExtendedKeyUsage(cert,&cursor);if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509OidsCopyV1(xl6X509CursorContentV1(cursor.Items),limit):NULL;
}
