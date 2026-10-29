#include "xl6_x509_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509CertificateDestroyV1(void* raw)
{
    XL6_X509CertificateV1* self=raw;
    if(self){xl6X509FreeV1(self->detached);xl6X509FreeV1(self);}
}
void* xl6X509CertificateCreateSpanV1(xbytesview input)
{
    if(!xl6X509SpanV1(input))return NULL;
    if(input.Size>SIZE_MAX-sizeof(XL6_X509CertificateV1)) {
        xl6X509FailV1(XERR_RANGE,"X.509 allocation size overflow");return NULL;
    }
    XL6_X509CertificateV1* self=xrtCalloc(1,sizeof(*self)+input.Size);
    if(!self)return NULL;
    memcpy(self->inlineDer,input.Data,input.Size);
    if(xl6X509OutcomeV1(xrtX509Parse(self->inlineDer,input.Size,&self->view)))return self;
    xl6X509CertificateDestroyV1(self);return NULL;
}
void* xl6X509CertificateTakeV1(bytes* slot,size_t size)
{
    if(!slot){xl6X509FailV1(XERR_ARGUMENT,"missing owned X.509 input slot");return NULL;}
    bytes data=*slot;*slot=NULL;
    if(!xl6X509SpanV1((xbytesview){data,size})){xl6X509FreeV1(data);return NULL;}
    XL6_X509CertificateV1* self=xrtCalloc(1,sizeof(*self));
    if(!self){xl6X509FreeV1(data);return NULL;}
    self->detached=data;
    if(xl6X509OutcomeV1(xrtX509Parse(data,size,&self->view)))return self;
    xl6X509CertificateDestroyV1(self);return NULL;
}
void* xl6X509CertificateFromDerV1(const xvalue* value,int64_t limit)
{
    xbytesview input;
    return xl6X509InputV1(value,&input) && xl6X509LimitV1(input.Size,limit,"certificate DER exceeds input budget")?
        xl6X509CertificateCreateSpanV1(input):NULL;
}
static const xx509cert* view(void* raw)
{
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing Certificate receiver");return NULL;}
    return &((const XL6_X509CertificateV1*)raw)->view;
}
#define FIELD(Name,Field) xvalue* xl6X509Certificate##Name##V1(void* raw) \
{const xx509cert* cert=view(raw);return cert?xl6X509BytesV1(cert->Field):NULL;}
FIELD(ToDer,Raw)
FIELD(Serial,Serial)
FIELD(Tbs,Tbs)
FIELD(Issuer,Issuer)
FIELD(Subject,Subject)
FIELD(Spki,SubjectPublicKeyInfo)
FIELD(Extensions,Extensions)
FIELD(Signature,Signature)
FIELD(SignatureOid,SignatureAlgorithm.Oid)
FIELD(SignatureParameters,SignatureAlgorithm.Parameters)
#undef FIELD
int64_t xl6X509CertificateVersionV1(void* raw)
{const xx509cert* cert=view(raw);return cert?(int64_t)cert->Version:0;}
int64_t xl6X509CertificateNotBeforeV1(void* raw)
{const xx509cert* cert=view(raw);return cert?cert->NotBefore:0;}
int64_t xl6X509CertificateNotAfterV1(void* raw)
{const xx509cert* cert=view(raw);return cert?cert->NotAfter:0;}
bool xl6X509CertificateValidAtV1(void* raw,int64_t time)
{const xx509cert* cert=view(raw);return cert && xrtX509ValidAt(cert,time);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509CertificateHandlePolicyV1Get,xl6X509CertificateDestroyV1)
