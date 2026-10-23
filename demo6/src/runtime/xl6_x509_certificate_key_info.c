#include "xl6_x509_records_internal.h"

static const xx509cert* view(void* raw)
{
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing Certificate receiver");return NULL;}
    return &((XL6_X509CertificateV1*)raw)->view;
}
void* xl6X509CertificateKeyInfoV1(void* raw)
{
    const xx509cert* self=view(raw);xx509pubkey key;
    return self && xl6X509OutcomeV1(xrtX509PublicKey(self,&key))?xl6X509PublicKeyInfoCopyV1(self->SubjectPublicKeyInfo,&key):NULL;
}
