#include "xl6_tls_selector_internal.h"
void xl6TlsServerChoiceDestroyV1(void* raw)
{
    XL6_TlsServerChoiceV1* self=raw;if(!self)return;
    xrtTlsIdentityRelease(self->identity);xrtFree(self);
}
void* xl6TlsServerChoiceCreateV1(void* identity,uint64_t cookie)
{
    if(xrtGetError())return NULL;
    if(!identity){xl6TlsFailV1(XERR_ARGUMENT,"TLS server choice needs an identity");return NULL;}
    XL6_TlsServerChoiceV1* self=xrtCalloc(1,sizeof(*self));if(!self)return NULL;
    self->identity=xrtTlsIdentityRetain(identity);self->cookie=cookie;
    if(!self->identity){xrtFree(self);return NULL;}return self;
}
static const XL6_TlsServerChoiceV1* checked(const void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS server choice");
    return raw;
}
void* xl6TlsServerChoiceWithProtocolIndexV1(void* raw,int64_t index)
{
    const XL6_TlsServerChoiceV1* self=checked(raw);if(!self)return NULL;
    if(index<-1||(index>=0&&(uint64_t)index>=SIZE_MAX)){
        xl6TlsFailV1(XERR_RANGE,"TLS protocol index must be -1 or a representable server protocol index");return NULL;}
    XL6_TlsServerChoiceV1* copy=xl6TlsServerChoiceCreateV1(self->identity,self->cookie);
    if(copy){copy->hasProtocolIndex=true;copy->protocolIndex=index;}return copy;
}
void* xl6TlsServerChoiceIdentityV1(const void* raw)
{const XL6_TlsServerChoiceV1* self=checked(raw);return self?xrtTlsIdentityRetain(self->identity):NULL;}
uint64_t xl6TlsServerChoiceCookieV1(const void* raw)
{const XL6_TlsServerChoiceV1* self=checked(raw);return self?self->cookie:0;}
bool xl6TlsServerChoiceHasProtocolIndexV1(const void* raw)
{const XL6_TlsServerChoiceV1* self=checked(raw);return self&&self->hasProtocolIndex;}
int64_t xl6TlsServerChoiceProtocolIndexV1(const void* raw)
{
    const XL6_TlsServerChoiceV1* self=checked(raw);if(!self)return 0;
    if(!self->hasProtocolIndex){xl6TlsFailV1(XERR_STATE,"TLS server choice uses automatic protocol selection");return 0;}
    return self->protocolIndex;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsServerChoiceHandlePolicyV1Get,xl6TlsServerChoiceDestroyV1)
