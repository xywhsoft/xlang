#include "xl6_tls_role_context_internal.h"
void xl6TlsServerContextDestroyV1(void* raw)
{
    XL6_TlsServerContextV1* self=raw;if(!self)return;
    xerror* prior=xrtTakeError();xrtTlsContextRelease(self->parameters);xrtTlsIdentityRelease(self->identity);
    xrtFree(self->protocols);xrtFree(self);if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
void* xl6TlsServerContextFromV1(xtlscontext* parameters,xtlsidentity* identity,XL6_TlsProtocolsV1* protocols,bool requireProtocol)
{
    if(xrtGetError()){xrtFree(protocols);return NULL;}
    if(!identity || (requireProtocol && !protocols)){
        xrtFree(protocols);xl6TlsFailV1(XERR_ARGUMENT,"TLS server context needs an identity and any required ALPN list");return NULL;}
    XL6_TlsServerContextV1* self=xrtCalloc(1,sizeof(*self));if(!self){xrtFree(protocols);return NULL;}
    self->protocols=protocols;self->requireProtocol=requireProtocol;
    self->parameters=parameters?xrtTlsContextRetain(parameters):xrtTlsContextCreate(NULL);
    if(self->parameters)self->identity=xrtTlsIdentityRetain(identity);
    if(!self->parameters || !self->identity){xl6TlsServerContextDestroyV1(self);return NULL;}
    return self;
}
static const XL6_TlsServerContextV1* checked(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS server context");
    return raw;
}
void* xl6TlsServerContextDefaultV1(void* identity){return xl6TlsServerContextFromV1(NULL,identity,NULL,false);}
void* xl6TlsServerContextCreateV1(void* parameters,void* identity)
{
    if(!parameters){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS parameters");return NULL;}
    return xl6TlsServerContextFromV1(parameters,identity,NULL,false);
}
void* xl6TlsServerContextParametersV1(void* raw)
{const XL6_TlsServerContextV1* self=checked(raw);return self?xrtTlsContextRetain(self->parameters):NULL;}
void* xl6TlsServerContextIdentityV1(void* raw)
{const XL6_TlsServerContextV1* self=checked(raw);return self?xrtTlsIdentityRetain(self->identity):NULL;}
int64_t xl6TlsServerContextProtocolCountV1(void* raw)
{const XL6_TlsServerContextV1* self=checked(raw);return self && self->protocols?(int64_t)self->protocols->count:0;}
xvalue* xl6TlsServerContextProtocolAtV1(void* raw,int64_t index)
{const XL6_TlsServerContextV1* self=checked(raw);return self?xl6TlsProtocolsAtV1(self->protocols,index):NULL;}
bool xl6TlsServerContextRequiresProtocolV1(void* raw)
{const XL6_TlsServerContextV1* self=checked(raw);return self && self->requireProtocol;}
void* xl6TlsServerContextWithParametersV1(void* raw,void* parameters)
{
    const XL6_TlsServerContextV1* self=checked(raw);XL6_TlsProtocolsV1* protocols=NULL;
    if(!self)return NULL;
    if(!parameters){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS parameters");return NULL;}
    if(!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    return xl6TlsServerContextFromV1(parameters,self->identity,protocols,self->requireProtocol);
}
void* xl6TlsServerContextWithIdentityV1(void* raw,void* identity)
{
    const XL6_TlsServerContextV1* self=checked(raw);XL6_TlsProtocolsV1* protocols=NULL;
    if(!self || !xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    return xl6TlsServerContextFromV1(self->parameters,identity,protocols,self->requireProtocol);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsServerContextHandlePolicyV1Get,xl6TlsServerContextDestroyV1)
