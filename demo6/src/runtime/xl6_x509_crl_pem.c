#include "xl6_x509_internal.h"

void* xl6X509CrlFromPemV1(xstrview input,int64_t textLimit,int64_t derLimit)
{
    bytes data=NULL;size_t size=0;
    if(!xl6X509PemDecodeV1(input,"X509 CRL",textLimit,derLimit,&data,&size))return NULL;
    return xl6X509CrlTakeV1(&data,size);
}
xstrview xl6X509CrlToPemV1(void* raw,int64_t limit)
{
    const XL6_X509CrlV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing CRL receiver");return (xstrview){0};}
    return xl6X509PemEncodeV1(self->view.Raw,"X509 CRL",limit);
}
