#include "xl6_x509_store_internal.h"
static const XL6_X509StoreVersionV1* checked(void* raw)
{if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing trust store snapshot");return raw;}
void* xl6X509StoreSnapshotCertificateV1(void* raw,int64_t index)
{
    const XL6_X509StoreVersionV1* self=checked(raw);size_t selected;
    if(!self || !xl6X509IndexV1(index,xrtX509StoreCount(self->value),&selected))return NULL;
    const xx509cert* value=xrtX509StoreCertificate(self->value,selected);
    return value?xl6X509CertificateCreateSpanV1(value->Raw):NULL;
}
void* xl6X509StoreSnapshotAnchorV1(void* raw,int64_t index)
{
    const XL6_X509StoreVersionV1* self=checked(raw);size_t selected;
    if(!self || !xl6X509IndexV1(index,xrtX509StoreCount(self->value),&selected))return NULL;
    const xx509cert* value=xrtX509StoreCertificate(self->value,selected);
    if(!value)return NULL;
    XL6_X509CertificateV1 certificate={.view=*value};return xl6X509AnchorCreateV1(&certificate);
}
