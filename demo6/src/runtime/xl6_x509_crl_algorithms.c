#include "xl6_x509_records_internal.h"

static const xx509crl* view(void* raw)
{
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing Crl receiver");return NULL;}
    return &((XL6_X509CrlV1*)raw)->view;
}
void* xl6X509CrlSignatureAlgorithmV1(void* raw)
{const xx509crl* self=view(raw);return self?xl6X509AlgorithmCopyV1(&self->SignatureAlgorithm):NULL;}
void* xl6X509CrlTbsSignatureAlgorithmV1(void* raw)
{const xx509crl* self=view(raw);return self?xl6X509AlgorithmCopyV1(&self->TbsSignature):NULL;}
