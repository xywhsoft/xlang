#include "xl6_x509_crl_policy_internal.h"
#include "src/api/xl_foreign_error.h"

static void targetDrop(XL6_X509RevocationTargetV1* target)
{if(target && xrtAtomic64FetchSub(&target->references,1,XMEMORY_ACQ_REL)==1)xl6X509FreeV1(target);}
void xl6X509RevocationCheckDestroyV1(void* raw)
{
    XL6_X509RevocationCheckV1* self=raw;
    if(self){targetDrop(self->target);xl6X509RevocationDestroyV1(self->result);xl6X509FreeV1(self);}
}
void* xl6X509RevocationCheckCreateV1(void* certificate)
{
    if(!certificate){xl6X509FailV1(XERR_ARGUMENT,"revocation aggregation needs a target certificate");return NULL;}
    xbytesview source=((const XL6_X509CertificateV1*)certificate)->view.Raw;
    if(!xl6X509SpanV1(source))return NULL;
    if(source.Size>SIZE_MAX-sizeof(XL6_X509RevocationTargetV1)) {
        xl6X509FailV1(XERR_RANGE,"revocation target allocation overflow");return NULL;
    }
    XL6_X509RevocationCheckV1* self=xrtCalloc(1,sizeof(*self));if(!self)return NULL;
    self->target=xrtCalloc(1,sizeof(*self->target)+source.Size);
    if(self->target) {
        xrtAtomic64Init(&self->target->references,1);
        memcpy(self->target->data,source.Data,source.Size);
        if(xl6X509OutcomeV1(xrtX509Parse(self->target->data,source.Size,&self->target->value))) {
            xrtX509RevocationInit(&self->value);return self;
        }
    }
    xl6X509RevocationCheckDestroyV1(self);return NULL;
}
static void* with(void* raw,void* crl,bool set)
{
    const XL6_X509RevocationCheckV1* self=raw;
    if(!self || !crl){xl6X509FailV1(XERR_ARGUMENT,"revocation update needs check and validated source");return NULL;}
    if(self->value.Determined){xl6X509FailV1(XERR_STATE,"revocation check is already determined");return NULL;}
    const XL6_X509ValidatedCrlV1* policy=set?((const XL6_X509CrlSetV1*)crl)->base:crl;
    if(self->hasPolicy && (self->time!=policy->time || self->skew!=policy->skew || self->flags!=policy->flags)) {
        xl6X509FailV1(XERR_ARGUMENT,"revocation sources must share one policy time, skew and flags");return NULL;
    }
    xx509revocation observation={0};xx509result queried;xx509crlversion version;
    if(set) {
        const XL6_X509CrlSetV1* source=crl;
        queried=xrtX509CrlSetCheck(&source->value,&self->target->value,&observation);
        version=source->base->crl.Version;
        uintptr_t begin=(uintptr_t)source->delta->crl.Raw.Data,p=(uintptr_t)observation.Entry.Raw.Data;
        if(queried==X509_VALUE && p>=begin && p-begin<source->delta->crl.Raw.Size)version=source->delta->crl.Version;
    } else {
        const XL6_X509ValidatedCrlV1* source=crl;
        queried=xrtX509CrlCheck(&source->value,&self->target->value,&observation);version=source->crl.Version;
    }
    if(!xl6X509OutcomeV1(queried!=X509_ERROR))return NULL;
    xx509revocationcheck next=self->value;
    if(queried==X509_VALUE && !xl6X509OutcomeV1(xrtX509RevocationUpdate(&next,&observation)!=X509_ERROR))return NULL;
    XL6_X509RevocationCheckV1* copy=xrtCalloc(1,sizeof(*copy));if(!copy)return NULL;
    if(!xl6X509PolicyRetainV1(&self->target->references)){xl6X509FreeV1(copy);return NULL;}
    copy->target=self->target;copy->value=next;copy->hasPolicy=true;
    copy->time=policy->time;copy->skew=policy->skew;copy->flags=policy->flags;
    if(next.Determined) {
        xx509revocation result={0};
        if(!xl6X509OutcomeV1(xrtX509RevocationResult(&next,&result)==X509_VALUE) ||
            !(copy->result=xl6X509RevocationCopyV1(&result,version,true))) {
            xl6X509RevocationCheckDestroyV1(copy);return NULL;
        }
        copy->value.Revocation=copy->result->value;
    }
    return copy;
}
void* xl6X509RevocationCheckWithCrlV1(void* raw,void* crl){return with(raw,crl,false);}
void* xl6X509RevocationCheckWithSetV1(void* raw,void* set){return with(raw,set,true);}
void* xl6X509RevocationCheckResultV1(void* raw)
{
    const XL6_X509RevocationCheckV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing revocation check");return NULL;}
    if(self->result)return xl6X509RevocationRefV1(self->result);
    xx509revocation partial={.CoveredReasons=self->value.CoveredReasons};
    return xl6X509RevocationCopyV1(&partial,X509_CRL_VERSION_2,false);
}
bool xl6X509RevocationCheckDeterminedV1(void* raw)
{
    const XL6_X509RevocationCheckV1* self=raw;
    if(!self)xl6X509FailV1(XERR_ARGUMENT,"missing revocation check");
    return self && self->value.Determined;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509RevocationCheckHandlePolicyV1Get,xl6X509RevocationCheckDestroyV1)
