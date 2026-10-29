#include "xl6_x509_crl_policy_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509ValidatedCrlDestroyV1(void* raw)
{
    XL6_X509ValidatedCrlV1* self=raw;
    if(self&&xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)==1)xl6X509FreeV1(self);
}
void* xl6X509ValidatedCrlRefV1(void* raw)
{
    XL6_X509ValidatedCrlV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing validated CRL");return NULL;}
    return xl6X509PolicyRetainV1(&self->references)?self:NULL;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509ValidatedCrlHandlePolicyV1Get,xl6X509ValidatedCrlDestroyV1)
