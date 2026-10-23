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
