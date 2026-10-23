#include "xl6_x509_crl_policy_internal.h"
#include "src/api/xl_foreign_error.h"

bool xl6X509PolicyRetainV1(xatomic64* references)
{
    uint64 count=xrtAtomic64Load(references,XMEMORY_RELAXED);
    for(;;) {
        if(!count || count==UINT64_MAX)
            return xl6X509FailV1(XERR_RANGE,"X.509 native owner count is exhausted");
        if(xrtAtomic64CompareExchange(references,&count,count+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;
    }
}
bool xl6X509CrlPolicyConfigV1(int64_t time,int64_t skew,uint64_t flags,
    xx509crlcriticalproc critical,void* data,xx509crlconfig* output)
{
    const uint64 known=X509_CRL_REQUIRE_NEXT_UPDATE|X509_CRL_REQUIRE_NUMBER|
        X509_CRL_REQUIRE_AUTHORITY_KEY_ID|X509_CRL_REQUIRE_KEY_IDENTIFIER|
        X509_CRL_REQUIRE_KEY_USAGE|X509_CRL_ALLOW_SHA1;
    if(!output)return xl6X509FailV1(XERR_ARGUMENT,"missing CRL policy output");
    if(skew<0)return xl6X509FailV1(XERR_RANGE,"CRL clock skew must be nonnegative microseconds");
    if((flags&~known) || ((flags&X509_CRL_REQUIRE_KEY_IDENTIFIER) && !(flags&X509_CRL_REQUIRE_AUTHORITY_KEY_ID)))
        return xl6X509FailV1(XERR_ARGUMENT,"invalid CRL policy flags");
    *output=(xx509crlconfig){.Time=time,.Skew=(uint64)skew,.Flags=(uint32)flags,.Critical=critical,.UserData=data};
    return true;
}
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
