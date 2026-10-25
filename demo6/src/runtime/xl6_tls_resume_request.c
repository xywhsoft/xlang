#include "xl6_tls_resume_request.h"
#include "xl6_tls_internal.h"
#include <string.h>
struct XL6_TlsResumeRequestV1 {
    XL6_TlsServerRequestV1* hello;
    xbytesview ticket;
    uint32 age;
};
XL6_TlsResumeRequestV1* xl6TlsResumeRequestCopyV1(const xtlsserverresumerequest* request)
{
    if(xrtGetError())return NULL;
    if(!request||(!request->Ticket.Data&&request->Ticket.Size)){
        xl6TlsFailV1(XERR_ARGUMENT,"missing TLS resume request or ticket bytes");return NULL;}
    if(request->Ticket.Size>SIZE_MAX-sizeof(XL6_TlsResumeRequestV1)){
        xl6TlsFailV1(XERR_RANGE,"TLS resume request allocation overflows");return NULL;}
    XL6_TlsServerRequestV1* hello=xl6TlsServerRequestCopyV1(request->ServerName,request->Protocols);
    if(!hello)return NULL;
    XL6_TlsResumeRequestV1* self=xrtMalloc(sizeof(*self)+request->Ticket.Size);
    if(!self){xl6TlsServerRequestDestroyV1(hello);return NULL;}
    uint8* data=(uint8*)(self+1);self->hello=hello;self->age=request->Age;
    self->ticket=(xbytesview){request->Ticket.Size?data:NULL,request->Ticket.Size};
    if(request->Ticket.Size)memcpy(data,request->Ticket.Data,request->Ticket.Size);
    return self;
}
void xl6TlsResumeRequestDestroyV1(void* raw)
{XL6_TlsResumeRequestV1* self=raw;if(self){xl6TlsServerRequestDestroyV1(self->hello);xrtFree(self);}}
static const XL6_TlsResumeRequestV1* checked(const void* raw)
{if(xrtGetError())return NULL;if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS resume request");return raw;}
xvalue* xl6TlsResumeRequestNameV1(const void* raw)
{const XL6_TlsResumeRequestV1* self=checked(raw);return self?xl6TlsServerRequestNameV1(self->hello):NULL;}
int64_t xl6TlsResumeRequestProtocolCountV1(const void* raw)
{const XL6_TlsResumeRequestV1* self=checked(raw);return self?xl6TlsServerRequestProtocolCountV1(self->hello):0;}
xvalue* xl6TlsResumeRequestProtocolAtV1(const void* raw,int64_t at)
{const XL6_TlsResumeRequestV1* self=checked(raw);return self?xl6TlsServerRequestProtocolAtV1(self->hello,at):NULL;}
xvalue* xl6TlsResumeRequestTicketV1(const void* raw)
{const XL6_TlsResumeRequestV1* self=checked(raw);return self?xrtValueBytes(self->ticket):NULL;}
uint64_t xl6TlsResumeRequestAgeV1(const void* raw)
{const XL6_TlsResumeRequestV1* self=checked(raw);return self?self->age:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsResumeRequestHandlePolicyV1Get,xl6TlsResumeRequestDestroyV1)
