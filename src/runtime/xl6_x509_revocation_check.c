#include "xl6_x509_crl_policy_internal.h"
#include "xl6_x509_path_internal.h"
#include "src/api/xl_foreign_error.h"

static void targetDrop(XL6_X509RevocationTargetV1* target)
{if(target && xrtAtomic64FetchSub(&target->references,1,XMEMORY_ACQ_REL)==1)xl6X509FreeV1(target);}
void xl6X509RevocationCheckDestroyV1(void* raw)
{
    XL6_X509RevocationCheckV1* self=raw;
    if(self){targetDrop(self->target);xl6X509RevocationDestroyV1(self->result);
        xl6X509RevocationEvidenceDropV1(self->evidence);xl6X509FreeV1(self);}
}
XL6_X509RevocationEvidenceV1* xl6X509RevocationEvidenceRefV1(XL6_X509RevocationEvidenceV1* self)
{return self&&xl6X509PolicyRetainV1(&self->references)?self:NULL;}
void xl6X509RevocationEvidenceDropV1(XL6_X509RevocationEvidenceV1* self)
{
    /* Iterative release: a long persistent aggregation cannot overflow the C
     * stack. No evidence can point forward or contain a language callback. */
    while(self&&xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)==1){
        XL6_X509RevocationEvidenceV1* previous=self->previous;
        xl6X509ValidatedCrlDestroyV1(self->base);xl6X509ValidatedCrlDestroyV1(self->delta);
        xl6X509PathDestroyV1(self->signerPath);xl6X509FreeV1(self);self=previous;
    }
}
XL6_X509RevocationEvidenceV1* xl6X509RevocationEvidenceAddV1(
    XL6_X509RevocationEvidenceV1* previous,void* raw,bool set,XL6_X509PathV1* path)
{
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"revocation evidence needs a validated source");return NULL;}
    const XL6_X509CrlSetV1* pair=set?raw:NULL;
    size_t added=set?2:1;
    if(previous&&previous->count>SIZE_MAX-added){xl6X509FailV1(XERR_RANGE,"revocation source count overflows");return NULL;}
    XL6_X509RevocationEvidenceV1* self=xrtCalloc(1,sizeof(*self));if(!self)return NULL;
    xrtAtomic64Init(&self->references,1);self->count=(previous?previous->count:0)+added;
    if(previous&&!(self->previous=xl6X509RevocationEvidenceRefV1(previous)))goto fail;
    self->base=xl6X509ValidatedCrlRefV1(set?(void*)pair->base:raw);if(!self->base)goto fail;
    if(set&&!(self->delta=xl6X509ValidatedCrlRefV1(pair->delta)))goto fail;
    if(path&&!(self->signerPath=xl6X509PathRefV1(path)))goto fail;
    return self;
fail:
    xl6X509RevocationEvidenceDropV1(self);return NULL;
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
            return self; /* calloc initializes the pinned native zero state. */
        }
    }
    xl6X509RevocationCheckDestroyV1(self);return NULL;
}
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
