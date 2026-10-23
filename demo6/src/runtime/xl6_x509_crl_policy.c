#include "xl6_x509_crl_policy_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509ValidatedCrlDestroyV1(void* raw)
{
    XL6_X509ValidatedCrlV1* self=raw;
    if(self && xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)==1)xl6X509FreeV1(self);
}
void* xl6X509ValidatedCrlRefV1(void* raw)
{
    XL6_X509ValidatedCrlV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing validated CRL");return NULL;}
    return xl6X509PolicyRetainV1(&self->references)?self:NULL;
}
void* xl6X509ValidatedCrlCreateV1(void* crl,void* issuer,const xx509crlconfig* config)
{
    if(!crl || !issuer || !config){xl6X509FailV1(XERR_ARGUMENT,"CRL validation needs CRL, issuer and explicit policy");return NULL;}
    const xbytesview left=((const XL6_X509CrlV1*)crl)->view.Raw;
    const xbytesview right=((const XL6_X509CertificateV1*)issuer)->view.Raw;
    if(!xl6X509SpanV1(left) || !xl6X509SpanV1(right))return NULL;
    if(left.Size>SIZE_MAX-sizeof(XL6_X509ValidatedCrlV1) || right.Size>SIZE_MAX-sizeof(XL6_X509ValidatedCrlV1)-left.Size) {
        xl6X509FailV1(XERR_RANGE,"validated CRL allocation size overflow");return NULL;
    }
    XL6_X509ValidatedCrlV1* self=xrtCalloc(1,sizeof(*self)+left.Size+right.Size);
    if(!self)return NULL;
    xrtAtomic64Init(&self->references,1);
    memcpy(self->data,left.Data,left.Size);memcpy(self->data+left.Size,right.Data,right.Size);
    bool ok=xrtX509CrlParse(self->data,left.Size,&self->crl) &&
        xrtX509Parse(self->data+left.Size,right.Size,&self->issuer) &&
        xrtX509CrlValidate(&self->crl,&self->issuer,config,&self->value);
    if(xl6X509OutcomeV1(ok)) {
        self->time=config->Time;self->skew=config->Skew;self->flags=config->Flags;
        return self;
    }
    xl6X509ValidatedCrlDestroyV1(self);return NULL;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509ValidatedCrlHandlePolicyV1Get,xl6X509ValidatedCrlDestroyV1)
