#include "xl6_x509_crl_policy_internal.h"
static void* with(void* raw,void* crl,bool set,void* signerPath)
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
    if(queried==X509_VALUE){
        copy->evidence=xl6X509RevocationEvidenceAddV1(self->evidence,crl,set,signerPath);
        if(!copy->evidence){xl6X509RevocationCheckDestroyV1(copy);return NULL;}
    }else if(self->evidence&&!(copy->evidence=xl6X509RevocationEvidenceRefV1(self->evidence))){
        xl6X509RevocationCheckDestroyV1(copy);return NULL;
    }
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
void* xl6X509RevocationCheckWithCrlV1(void* raw,void* crl){return with(raw,crl,false,NULL);}
void* xl6X509RevocationCheckWithSetV1(void* raw,void* set){return with(raw,set,true,NULL);}
void* xl6X509RevocationCheckWithCrlPathV1(void* raw,void* crl,void* path)
{if(!path){xl6X509FailV1(XERR_ARGUMENT,"missing CRL signer path");return NULL;}return with(raw,crl,false,path);}
void* xl6X509RevocationCheckWithSetPathV1(void* raw,void* set,void* path)
{if(!path){xl6X509FailV1(XERR_ARGUMENT,"missing CRL signer path");return NULL;}return with(raw,set,true,path);}
