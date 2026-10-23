#include "xl6_x509_profiles_internal.h"
void xl6X509NameConstraintsCheckV1(void* raw,void* certificate)
{
    const XL6_X509NameConstraintsV1* self=xl6X509ProfileCheckedV1(raw);
    const xx509cert* cert=xl6X509ProfileCertificateV1(certificate);
    if(self && cert)xl6X509OutcomeV1(xrtX509NameConstraintsCheck(&self->value,cert));
}
