#include "xl6_tls_ticket_result.h"
void xl6TlsTicketResultDestroyV1(void* raw)
{
    XL6_TlsTicketResultV1* self=raw;if(!self)return;
    xerror* prior=xrtTakeError();xrtTlsResumeRelease(self->resume);xrtFree(self);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
static const XL6_TlsTicketResultV1* checked(const void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS ticket result");
    return raw;
}
int64_t xl6TlsTicketResultStatusV1(const void* raw)
{const XL6_TlsTicketResultV1* self=checked(raw);return self?self->status:XTLS_ERROR;}
void* xl6TlsTicketResultResumeV1(const void* raw)
{
    const XL6_TlsTicketResultV1* self=checked(raw);if(!self||!self->resume)return NULL;
    void* result=xrtTlsResumeRetain(self->resume);
    if(!result)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsTicketResultHandlePolicyV1Get,xl6TlsTicketResultDestroyV1)
