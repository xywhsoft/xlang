#include "xl6_x509_records_internal.h"

static const xx509cert* view(void* raw)
{
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing Certificate receiver");return NULL;}
    return &((XL6_X509CertificateV1*)raw)->view;
}
void* xl6X509CertificateIssuerNameV1(void* raw,int64_t limit)
{const xx509cert* self=view(raw);return self?xl6X509NameCopyV1(self->Issuer,limit):NULL;}
void* xl6X509CertificateSubjectNameV1(void* raw,int64_t limit)
{const xx509cert* self=view(raw);return self?xl6X509NameCopyV1(self->Subject,limit):NULL;}
