#include "xl6_tls_role_context_internal.h"
void xl6TlsServerContextDestroyV1(void* raw)
{
    XL6_TlsServerContextV1* self=raw;if(!self)return;
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,false))abort();
    xrtTlsContextRelease(self->parameters);xrtTlsIdentityRelease(self->identity);
    xl6TlsSelectorDropV1(&self->selector);
    xrtFree(self->protocols);xrtFree(self);
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);else xrtErrorFree(result.error);
}
void* xl6TlsServerContextFromV1(xtlscontext* parameters,xtlsidentity* identity,XL6_TlsProtocolsV1* protocols,bool requireProtocol)
{return xl6TlsServerContextFromSelectorV1(parameters,identity,protocols,requireProtocol,(XL6_TlsSelectorV1){0});}
void* xl6TlsServerContextFromSelectorV1(xtlscontext* parameters,xtlsidentity* identity,XL6_TlsProtocolsV1* protocols,bool requireProtocol,XL6_TlsSelectorV1 selector)
{
    if(xrtGetError()){xrtFree(protocols);return NULL;}
    if((!identity&&!selector.data) || (requireProtocol && !protocols)){
        xrtFree(protocols);xl6TlsFailV1(XERR_ARGUMENT,"TLS server context needs an identity and any required ALPN list");return NULL;}
    XL6_TlsServerContextV1* self=xrtCalloc(1,sizeof(*self));if(!self){xrtFree(protocols);return NULL;}
    self->protocols=protocols;self->requireProtocol=requireProtocol;
    self->parameters=parameters?xrtTlsContextRetain(parameters):xrtTlsContextCreate(NULL);
    if(self->parameters&&identity)self->identity=xrtTlsIdentityRetain(identity);
    if(!self->parameters || (identity&&!self->identity)){xl6TlsServerContextDestroyV1(self);return NULL;}
    self->selector=xl6TlsSelectorRefV1(selector);
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
{
    const XL6_TlsServerContextV1* self=checked(raw);if(!self)return NULL;
    if(!self->identity){xl6TlsFailV1(XERR_STATE,"dynamic TLS context has no static identity");return NULL;}
    return xrtTlsIdentityRetain(self->identity);
}
int64_t xl6TlsServerContextProtocolCountV1(void* raw)
{const XL6_TlsServerContextV1* self=checked(raw);return self && self->protocols?(int64_t)self->protocols->count:0;}
xvalue* xl6TlsServerContextProtocolAtV1(void* raw,int64_t index)
{const XL6_TlsServerContextV1* self=checked(raw);return self?xl6TlsProtocolsAtV1(self->protocols,index):NULL;}
bool xl6TlsServerContextRequiresProtocolV1(void* raw)
{const XL6_TlsServerContextV1* self=checked(raw);return self && self->requireProtocol;}
bool xl6TlsServerContextHasSelectorV1(void* raw)
{const XL6_TlsServerContextV1* self=checked(raw);return self&&self->selector.data!=NULL;}
void* xl6TlsServerContextWithParametersV1(void* raw,void* parameters)
{
    const XL6_TlsServerContextV1* self=checked(raw);XL6_TlsProtocolsV1* protocols=NULL;
    if(!self)return NULL;
    if(!parameters){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS parameters");return NULL;}
    if(!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    return xl6TlsServerContextFromSelectorV1(parameters,self->identity,protocols,self->requireProtocol,self->selector);
}
void* xl6TlsServerContextWithIdentityV1(void* raw,void* identity)
{
    const XL6_TlsServerContextV1* self=checked(raw);XL6_TlsProtocolsV1* protocols=NULL;
    if(!self || !xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    return xl6TlsServerContextFromSelectorV1(self->parameters,identity,protocols,self->requireProtocol,self->selector);
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=xl6TlsServerContextHandlePolicyV1Get()->Ops)return false;
    const XL6_TlsServerContextV1* self=raw;
    return !self->selector.data||visit(self->selector.ops->Ownership(self->selector.data),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TlsServerContextHandlePolicyV1Get,xl6TlsServerContextDestroyV1,trace)
