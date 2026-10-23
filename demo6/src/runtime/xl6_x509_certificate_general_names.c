#include "xl6_x509_profiles_internal.h"
static void* names(void* raw,int64_t limit,bool issuer)
{
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);xx509gencursor cursor;
    if(!cert || !xl6X509LimitV1(0,limit,"invalid alternative name budget"))return NULL;
    xx509result status=issuer?xrtX509IssuerAltName(cert,&cursor):xrtX509SubjectAltName(cert,&cursor);
    if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509GeneralNamesCopyV1(xl6X509CursorContentV1(cursor.Items),limit):NULL;
}
void* xl6X509CertificateSubjectAltNamesV1(void* raw,int64_t limit){return names(raw,limit,false);}
void* xl6X509CertificateIssuerAltNamesV1(void* raw,int64_t limit){return names(raw,limit,true);}
