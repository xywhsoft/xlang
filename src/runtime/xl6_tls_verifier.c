#include "xl6_tls_verifier_internal.h"
void xl6TlsVerifierDestroyV1(void* raw)
{
    XL6_TlsVerifierV1* self=raw;if(!self)return;
    xerror* prior=xrtTakeError();xrtTlsVerifierRelease(self->value);xrtFree(self);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
XL6_TlsVerifierV1* xl6TlsVerifierCopyV1(const XL6_TlsVerifierV1* self)
{
    if(xrtGetError())return NULL;
    if(!self || !self->value || !self->policy){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS verifier");return NULL;}
    XL6_TlsVerifierV1* copy=xrtMalloc(sizeof(*copy));if(!copy)return NULL;
    copy->value=xrtTlsVerifierRetain(self->value);copy->policy=self->policy;
    if(!copy->value){xerror* error=xrtTakeError();xrtFree(copy);xrtSetErrorTake(error);return NULL;}
    return copy;
}
void* xl6TlsVerifierPolicyV1(void* raw)
{
    const XL6_TlsVerifierV1* self=raw;if(xrtGetError())return NULL;
    if(!self || !self->value || !self->policy){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS verifier");return NULL;}
    return xl6TlsVerificationPolicyCopyV1(self->policy);
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* actual=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&actual,&user)||!raw||user||
        actual!=xl6TlsVerifierHandlePolicyV1Get()->Ops)return false;
    const XL6_TlsVerifierV1* self=raw;
    return self->value&&visit(xrtTlsVerifierOwnership(self->value),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TlsVerifierHandlePolicyV1Get,xl6TlsVerifierDestroyV1,trace)
