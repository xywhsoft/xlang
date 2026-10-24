#include "xl6_tls_role_context_internal.h"
void xl6TlsClientContextDestroyV1(void* raw)
{
    XL6_TlsClientContextV1* self=raw;if(!self)return;
    xerror* prior=xrtTakeError();xrtTlsContextRelease(self->parameters);xrtTlsVerifierRelease(self->verifier.value);
    xrtFree(self->protocols);xrtFree(self);if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
void* xl6TlsClientContextFromV1(xtlscontext* parameters,const XL6_TlsVerifierV1* verifier,XL6_TlsProtocolsV1* protocols)
{
    if(xrtGetError()){xrtFree(protocols);return NULL;}
    if(!verifier || !verifier->value || !verifier->policy){
        xrtFree(protocols);xl6TlsFailV1(XERR_ARGUMENT,"TLS client context requires a trust verifier");return NULL;}
    XL6_TlsClientContextV1* self=xrtCalloc(1,sizeof(*self));if(!self){xrtFree(protocols);return NULL;}
    self->protocols=protocols;self->parameters=parameters?xrtTlsContextRetain(parameters):xrtTlsContextCreate(NULL);
    if(self->parameters){self->verifier.value=xrtTlsVerifierRetain(verifier->value);self->verifier.policy=verifier->policy;}
    if(!self->parameters || !self->verifier.value){xl6TlsClientContextDestroyV1(self);return NULL;}
    return self;
}
static const XL6_TlsClientContextV1* checked(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS client context");
    return raw;
}
void* xl6TlsClientContextDefaultV1(void* verifier){return xl6TlsClientContextFromV1(NULL,verifier,NULL);}
void* xl6TlsClientContextCreateV1(void* parameters,void* verifier)
{
    if(!parameters){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS parameters");return NULL;}
    return xl6TlsClientContextFromV1(parameters,verifier,NULL);
}
void* xl6TlsClientContextParametersV1(void* raw)
{const XL6_TlsClientContextV1* self=checked(raw);return self?xrtTlsContextRetain(self->parameters):NULL;}
void* xl6TlsClientContextVerifierV1(void* raw)
{const XL6_TlsClientContextV1* self=checked(raw);return self?xl6TlsVerifierCopyV1(&self->verifier):NULL;}
int64_t xl6TlsClientContextProtocolCountV1(void* raw)
{const XL6_TlsClientContextV1* self=checked(raw);return self && self->protocols?(int64_t)self->protocols->count:0;}
xvalue* xl6TlsClientContextProtocolAtV1(void* raw,int64_t index)
{const XL6_TlsClientContextV1* self=checked(raw);return self?xl6TlsProtocolsAtV1(self->protocols,index):NULL;}
void* xl6TlsClientContextWithParametersV1(void* raw,void* parameters)
{
    const XL6_TlsClientContextV1* self=checked(raw);XL6_TlsProtocolsV1* protocols=NULL;
    if(!self)return NULL;
    if(!parameters){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS parameters");return NULL;}
    if(!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    return xl6TlsClientContextFromV1(parameters,&self->verifier,protocols);
}
void* xl6TlsClientContextWithVerifierV1(void* raw,void* verifier)
{
    const XL6_TlsClientContextV1* self=checked(raw);XL6_TlsProtocolsV1* protocols=NULL;
    if(!self || !xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    return xl6TlsClientContextFromV1(self->parameters,verifier,protocols);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsClientContextHandlePolicyV1Get,xl6TlsClientContextDestroyV1)
