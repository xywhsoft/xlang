#include "xl6_x509_records_internal.h"

static const xx509cert* view(void* raw)
{
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing Certificate receiver");return NULL;}
    return &((XL6_X509CertificateV1*)raw)->view;
}
xvalue* xl6X509CertificateIssuerUniqueIdV1(void* raw)
{const xx509cert* self=view(raw);return self?xl6X509BytesV1(self->IssuerUniqueId):NULL;}
int64_t xl6X509CertificateIssuerUniqueUnusedV1(void* raw)
{const xx509cert* self=view(raw);return self?(int64_t)self->IssuerUniqueIdUnusedBits:0;}
bool xl6X509CertificateHasIssuerUniqueIdV1(void* raw)
{const xx509cert* self=view(raw);return self && self->IssuerUniqueId.Data!=NULL;}
xvalue* xl6X509CertificateSubjectUniqueIdV1(void* raw)
{const xx509cert* self=view(raw);return self?xl6X509BytesV1(self->SubjectUniqueId):NULL;}
int64_t xl6X509CertificateSubjectUniqueUnusedV1(void* raw)
{const xx509cert* self=view(raw);return self?(int64_t)self->SubjectUniqueIdUnusedBits:0;}
bool xl6X509CertificateHasSubjectUniqueIdV1(void* raw)
{const xx509cert* self=view(raw);return self && self->SubjectUniqueId.Data!=NULL;}
