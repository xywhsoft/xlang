#include "xl6_x509_crl_policy_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509CrlSetDestroyV1(void* raw)
{
    XL6_X509CrlSetV1* self=raw;
    if(!self)return;
    xl6X509ValidatedCrlDestroyV1(self->base);xl6X509ValidatedCrlDestroyV1(self->delta);
    xl6X509FreeV1(self);
}
void* xl6X509CrlSetCreateV1(void* base,void* delta)
{
    if(!base || !delta){xl6X509FailV1(XERR_ARGUMENT,"complete/delta combination needs two validated CRLs");return NULL;}
    const XL6_X509ValidatedCrlV1* left=base;const XL6_X509ValidatedCrlV1* right=delta;
    if(left->time!=right->time || left->skew!=right->skew || left->flags!=right->flags) {
        xl6X509FailV1(XERR_ARGUMENT,"complete/delta CRLs must use the same policy time, skew and flags");return NULL;
    }
    XL6_X509CrlSetV1* self=xrtCalloc(1,sizeof(*self));if(!self)return NULL;
    self->base=xl6X509ValidatedCrlRefV1(base);
    if(self->base)self->delta=xl6X509ValidatedCrlRefV1(delta);
    if(self->delta && xl6X509OutcomeV1(xrtX509CrlSetInit(&self->value,&self->base->value,&self->delta->value)))return self;
    xl6X509CrlSetDestroyV1(self);return NULL;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509CrlSetHandlePolicyV1Get,xl6X509CrlSetDestroyV1)
