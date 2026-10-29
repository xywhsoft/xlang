#include "xl6_x509_internal.h"

void* xl6X509CertificateFromPemV1(xstrview input,int64_t textLimit,int64_t derLimit)
{
    bytes data=NULL;size_t size=0;
    if(!xl6X509PemDecodeV1(input,"CERTIFICATE",textLimit,derLimit,&data,&size))return NULL;
    return xl6X509CertificateTakeV1(&data,size);
}
xstrview xl6X509CertificateToPemV1(void* raw,int64_t limit)
{
    const XL6_X509CertificateV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing Certificate receiver");return (xstrview){0};}
    return xl6X509PemEncodeV1(self->view.Raw,"CERTIFICATE",limit);
}
