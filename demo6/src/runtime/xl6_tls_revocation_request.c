#include "xl6_tls_revocation_internal.h"
#include "xl6_tls_internal.h"
#include "xl6_x509_internal.h"
XL6_TlsRevocationRequestV1* xl6TlsRevocationRequestCreateV1(
    XL6_TlsVerifiedPeerPacketV1* peer,size_t index,const XL6_TlsRevocationOptionsV1* options)
{
    if(xrtGetError())return NULL;
    if(!peer||!options||index>=xl6TlsVerifiedPeerPacketPathCountV1(peer)){
        xl6TlsFailV1(XERR_ARGUMENT,"revocation request needs a selected path target");return NULL;}
    XL6_TlsRevocationRequestV1* self=xrtMalloc(sizeof(*self));if(!self)return NULL;
    *self=(XL6_TlsRevocationRequestV1){NULL,index,*options};self->peer=xl6TlsVerifiedPeerPacketRefV1(peer);
    if(self->peer)return self;
    xrtFree(self);return NULL;
}
void xl6TlsRevocationRequestDestroyV1(void* raw)
{XL6_TlsRevocationRequestV1* self=raw;if(self){xl6TlsVerifiedPeerPacketDropV1(self->peer);xrtFree(self);}}
static const XL6_TlsRevocationRequestV1* checked(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS revocation request");
    return raw;
}
void* xl6TlsRevocationRequestPeerV1(void* raw)
{const XL6_TlsRevocationRequestV1* self=checked(raw);return self?xl6TlsVerifiedPeerPacketRefV1(self->peer):NULL;}
void* xl6TlsRevocationRequestTargetV1(void* raw)
{
    const XL6_TlsRevocationRequestV1* self=checked(raw);if(!self)return NULL;
    xbytesview der=xl6TlsVerifiedPeerPacketPathDerV1(self->peer,self->index);
    return xrtGetError()?NULL:xl6X509CertificateCreateSpanV1(der);
}
void* xl6TlsRevocationRequestIssuerV1(void* raw)
{
    const XL6_TlsRevocationRequestV1* self=checked(raw);if(!self)return NULL;
    xbytesview der=self->index+1<xl6TlsVerifiedPeerPacketPathCountV1(self->peer)?
        xl6TlsVerifiedPeerPacketPathDerV1(self->peer,self->index+1):xl6TlsVerifiedPeerPacketAnchorDerV1(self->peer);
    return xrtGetError()?NULL:xl6X509CertificateCreateSpanV1(der);
}
int64_t xl6TlsRevocationRequestIndexV1(void* raw)
{const XL6_TlsRevocationRequestV1* self=checked(raw);return self?(int64_t)self->index:0;}
int64_t xl6TlsRevocationRequestTimeV1(void* raw)
{const XL6_TlsRevocationRequestV1* self=checked(raw);return self?xl6TlsVerifiedPeerPacketTimeV1(self->peer):0;}
int64_t xl6TlsRevocationRequestSkewV1(void* raw)
{const XL6_TlsRevocationRequestV1* self=checked(raw);return self?self->options.skew:0;}
int64_t xl6TlsRevocationRequestFlagsV1(void* raw)
{const XL6_TlsRevocationRequestV1* self=checked(raw);return self?(int64_t)self->options.flags:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsRevocationRequestHandlePolicyV1Get,xl6TlsRevocationRequestDestroyV1)
