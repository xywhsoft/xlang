#include "xl6_tls_role_context_internal.h"
void xl6TlsClientContextDestroyV1(void* raw)
{
    XL6_TlsClientContextV1* self=raw;if(!self)return;
    xerror* prior=xrtTakeError();xrtTlsContextRelease(self->parameters);xrtTlsVerifierRelease(self->verifier.value);
    if(self->resume.value)self->resume.ops->Drop(self->resume.value);
    xrtFree(self->protocols);xrtFree(self);if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
void* xl6TlsClientContextFromV1(xtlscontext* parameters,const XL6_TlsVerifierV1* verifier,XL6_TlsProtocolsV1* protocols)
{return xl6TlsClientContextFromResumeV1(parameters,verifier,protocols,(XL6_TlsResumeOwnerV1){0});}
void* xl6TlsClientContextFromResumeV1(xtlscontext* parameters,const XL6_TlsVerifierV1* verifier,XL6_TlsProtocolsV1* protocols,XL6_TlsResumeOwnerV1 resume)
{return xl6TlsClientContextFromOptionsV1(parameters,verifier,protocols,resume,XL6_TLS_TICKET_CAPACITY_DEFAULT_V1,false);}
void* xl6TlsClientContextFromOptionsV1(xtlscontext* parameters,const XL6_TlsVerifierV1* verifier,XL6_TlsProtocolsV1* protocols,
    XL6_TlsResumeOwnerV1 resume,uint32_t capacity,bool required)
{
    if(xrtGetError()){xrtFree(protocols);return NULL;}
    if(capacity>XL6_TLS_TICKET_CAPACITY_MAX_V1||(required&&!resume.value)){
        xrtFree(protocols);xl6TlsFailV1(XERR_ARGUMENT,"invalid TLS ticket capacity or required-resume configuration");return NULL;}
    if(!verifier || !verifier->value || !verifier->policy){
        xrtFree(protocols);xl6TlsFailV1(XERR_ARGUMENT,"TLS client context requires a trust verifier");return NULL;}
    if(resume.value&&(!resume.ops||!resume.ops->Ref||!resume.ops->Drop)){
        xrtFree(protocols);xl6TlsFailV1(XERR_ARGUMENT,"TLS client resume needs permanent native operations");return NULL;}
    XL6_TlsClientContextV1* self=xrtCalloc(1,sizeof(*self));if(!self){xrtFree(protocols);return NULL;}
    self->protocols=protocols;self->parameters=parameters?xrtTlsContextRetain(parameters):xrtTlsContextCreate(NULL);
    if(self->parameters){self->verifier.value=xrtTlsVerifierRetain(verifier->value);self->verifier.policy=verifier->policy;}
    if(!self->parameters || !self->verifier.value){xl6TlsClientContextDestroyV1(self);return NULL;}
    if(resume.value){
        self->resume=(XL6_TlsResumeOwnerV1){resume.ops->Ref(resume.value),resume.ops};
        if(!self->resume.value){xl6TlsOutcomeV1(false);xl6TlsClientContextDestroyV1(self);return NULL;}
    }
    self->ticketCapacity=capacity;self->resumeRequired=required;
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
    return xl6TlsClientContextFromOptionsV1(parameters,&self->verifier,protocols,self->resume,self->ticketCapacity,self->resumeRequired);
}
void* xl6TlsClientContextWithVerifierV1(void* raw,void* verifier)
{
    const XL6_TlsClientContextV1* self=checked(raw);XL6_TlsProtocolsV1* protocols=NULL;
    if(!self || !xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    /* An explicit trust rebind invalidates a credential authenticated under
     * the previous verifier, even when the caller supplies the same snapshot. */
    return xl6TlsClientContextFromOptionsV1(self->parameters,verifier,protocols,(XL6_TlsResumeOwnerV1){0},self->ticketCapacity,false);
}
bool xl6TlsClientContextHasResumeV1(void* raw)
{const XL6_TlsClientContextV1* self=checked(raw);return self&&self->resume.value!=NULL;}
void* xl6TlsClientContextResumeV1(void* raw)
{
    const XL6_TlsClientContextV1* self=checked(raw);if(!self||!self->resume.value)return NULL;
    void* value=self->resume.ops->Ref(self->resume.value);
    if(!value)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return value;
}
void* xl6TlsClientContextWithoutResumeV1(void* raw)
{
    const XL6_TlsClientContextV1* self=checked(raw);XL6_TlsProtocolsV1* protocols=NULL;
    if(!self||!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    return xl6TlsClientContextFromOptionsV1(self->parameters,&self->verifier,protocols,(XL6_TlsResumeOwnerV1){0},self->ticketCapacity,false);
}
int64_t xl6TlsClientContextTicketCapacityV1(void* raw)
{const XL6_TlsClientContextV1* self=checked(raw);return self?self->ticketCapacity:0;}
bool xl6TlsClientContextResumeRequiredV1(void* raw)
{const XL6_TlsClientContextV1* self=checked(raw);return self&&self->resumeRequired;}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* actual=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&actual,&user)||!raw||user||
        actual!=xl6TlsClientContextHandlePolicyV1Get()->Ops)return false;
    const XL6_TlsClientContextV1* self=raw;
    return self->verifier.value&&visit(xrtTlsVerifierOwnership(self->verifier.value),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TlsClientContextHandlePolicyV1Get,xl6TlsClientContextDestroyV1,trace)
