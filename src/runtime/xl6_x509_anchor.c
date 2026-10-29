#include "xl6_x509_path_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509AnchorDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509AnchorCreateV1(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"trust anchor needs an explicit certificate");return NULL;}
    xbytesview source=((const XL6_X509CertificateV1*)raw)->view.Raw;
    if(!xl6X509SpanV1(source))return NULL;
    if(source.Size>SIZE_MAX-sizeof(XL6_X509AnchorV1)) {
        xl6X509FailV1(XERR_RANGE,"trust anchor allocation overflow");return NULL;
    }
    XL6_X509AnchorV1* self=xrtCalloc(1,sizeof(*self)+source.Size);
    if(!self)return NULL;
    memcpy(self->data,source.Data,source.Size);
    if(xl6X509OutcomeV1(xrtX509Parse(self->data,source.Size,&self->certificate) &&
        xrtX509Anchor(&self->certificate,&self->value)))return self;
    xl6X509AnchorDestroyV1(self);return NULL;
}
void* xl6X509AnchorCertificateV1(void* raw)
{
    const XL6_X509AnchorV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing trust anchor");return NULL;}
    return xl6X509CertificateCreateSpanV1(self->certificate.Raw);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509AnchorHandlePolicyV1Get,xl6X509AnchorDestroyV1)
