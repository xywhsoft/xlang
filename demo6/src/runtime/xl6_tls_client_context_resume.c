#include "xl6_tls_role_context_internal.h"
_Static_assert(XL6_TLS_TICKET_CAPACITY_DEFAULT_V1==XTLS_CLIENT_RESUME_LIMIT_DEFAULT,"pinned TLS ticket default drift");
_Static_assert(XL6_TLS_TICKET_CAPACITY_MAX_V1<=XTLS_CLIENT_RESUME_LIMIT_MAX,"language TLS ticket bound exceeds native capacity");
static void* withResume(void* raw,void* resume,bool required)
{
    if(xrtGetError())return NULL;
    const XL6_TlsClientContextV1* self=raw;XL6_TlsProtocolsV1* protocols=NULL;
    if(!self||!resume){xl6TlsFailV1(XERR_ARGUMENT,"TLS client resumption needs a context and credential");return NULL;}
    if(!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    static const XL6_TlsResumeOpsV1 ops={xrtTlsResumeRetain,xrtTlsResumeRelease};
    return xl6TlsClientContextFromOptionsV1(self->parameters,&self->verifier,protocols,(XL6_TlsResumeOwnerV1){resume,&ops},self->ticketCapacity,required);
}
void* xl6TlsClientContextWithResumeV1(void* raw,void* resume){return withResume(raw,resume,false);}
void* xl6TlsClientContextWithRequiredResumeV1(void* raw,void* resume){return withResume(raw,resume,true);}
void* xl6TlsClientContextWithTicketCapacityV1(void* raw,int64_t capacity)
{
    if(xrtGetError())return NULL;
    const XL6_TlsClientContextV1* self=raw;XL6_TlsProtocolsV1* protocols=NULL;
    if(!self){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS client context");return NULL;}
    if(capacity<0||capacity>XL6_TLS_TICKET_CAPACITY_MAX_V1){xl6TlsFailV1(XERR_RANGE,"TLS ticket capacity must be between 0 and 64");return NULL;}
    if(!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    return xl6TlsClientContextFromOptionsV1(self->parameters,&self->verifier,protocols,self->resume,(uint32_t)capacity,self->resumeRequired);
}
