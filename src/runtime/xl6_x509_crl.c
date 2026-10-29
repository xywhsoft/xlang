#include "xl6_x509_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509CrlDestroyV1(void* raw)
{
    XL6_X509CrlV1* self=raw;
    if(self){xl6X509FreeV1(self->detached);xl6X509FreeV1(self);}
}
void* xl6X509CrlCreateSpanV1(xbytesview input)
{
    if(!xl6X509SpanV1(input))return NULL;
    if(input.Size>SIZE_MAX-sizeof(XL6_X509CrlV1)) {
        xl6X509FailV1(XERR_RANGE,"CRL allocation size overflow");return NULL;
    }
    XL6_X509CrlV1* self=xrtCalloc(1,sizeof(*self)+input.Size);
    if(!self)return NULL;
    memcpy(self->inlineDer,input.Data,input.Size);
    if(xl6X509OutcomeV1(xrtX509CrlParse(self->inlineDer,input.Size,&self->view)))return self;
    xl6X509CrlDestroyV1(self);return NULL;
}
void* xl6X509CrlTakeV1(bytes* slot,size_t size)
{
    if(!slot){xl6X509FailV1(XERR_ARGUMENT,"missing owned CRL input slot");return NULL;}
    bytes data=*slot;*slot=NULL;
    if(!xl6X509SpanV1((xbytesview){data,size})){xl6X509FreeV1(data);return NULL;}
    XL6_X509CrlV1* self=xrtCalloc(1,sizeof(*self));
    if(!self){xl6X509FreeV1(data);return NULL;}
    self->detached=data;
    if(xl6X509OutcomeV1(xrtX509CrlParse(data,size,&self->view)))return self;
    xl6X509CrlDestroyV1(self);return NULL;
}
void* xl6X509CrlFromDerV1(const xvalue* value,int64_t limit)
{
    xbytesview input;
    return xl6X509InputV1(value,&input) && xl6X509LimitV1(input.Size,limit,"CRL DER exceeds input budget")?
        xl6X509CrlCreateSpanV1(input):NULL;
}
static const xx509crl* view(void* raw)
{
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing CRL receiver");return NULL;}
    return &((const XL6_X509CrlV1*)raw)->view;
}
#define FIELD(Name,Field) xvalue* xl6X509Crl##Name##V1(void* raw) \
{const xx509crl* crl=view(raw);return crl?xl6X509BytesV1(crl->Field):NULL;}
FIELD(ToDer,Raw)
FIELD(Tbs,Tbs)
FIELD(Issuer,Issuer)
FIELD(Revoked,Revoked)
FIELD(Extensions,Extensions)
FIELD(Signature,Signature)
FIELD(SignatureOid,SignatureAlgorithm.Oid)
FIELD(SignatureParameters,SignatureAlgorithm.Parameters)
#undef FIELD
int64_t xl6X509CrlVersionV1(void* raw)
{const xx509crl* crl=view(raw);return crl?(int64_t)crl->Version:0;}
int64_t xl6X509CrlThisUpdateV1(void* raw)
{const xx509crl* crl=view(raw);return crl?crl->ThisUpdate:0;}
bool xl6X509CrlHasNextUpdateV1(void* raw)
{const xx509crl* crl=view(raw);return crl && crl->HasNextUpdate;}
int64_t xl6X509CrlNextUpdateV1(void* raw)
{
    const xx509crl* crl=view(raw);if(!crl)return 0;
    if(!crl->HasNextUpdate){xl6X509FailV1(XERR_STATE,"CRL has no nextUpdate");return 0;}
    return crl->NextUpdate;
}
bool xl6X509CrlValidAtV1(void* raw,int64_t time)
{const xx509crl* crl=view(raw);return crl && xrtX509CrlValidAt(crl,time);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509CrlHandlePolicyV1Get,xl6X509CrlDestroyV1)
