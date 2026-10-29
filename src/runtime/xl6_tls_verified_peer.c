#include "xl6_tls_verified_peer_packet.h"
#include "xl6_tls_internal.h"
#include "xl6_text_internal.h"
#include "xl6_x509_internal.h"

void xl6TlsVerifiedPeerDestroyV1(void* raw){xl6TlsVerifiedPeerPacketDropV1(raw);}
int64_t xl6TlsVerifiedPeerRoleV1(void* raw){return xl6TlsVerifiedPeerPacketRoleV1(raw);}
int64_t xl6TlsVerifiedPeerTimeV1(void* raw){return xl6TlsVerifiedPeerPacketTimeV1(raw);}
xstrview xl6TlsVerifiedPeerNameV1(void* raw)
{
    xstrview view=xl6TlsVerifiedPeerPacketNameViewV1(raw);
    return xrtGetError()?xl6TextFailure:xl6TextCopy(view);
}
int64_t xl6TlsVerifiedPeerCertificateCountV1(void* raw)
{return (int64_t)xl6TlsVerifiedPeerPacketCertificateCountV1(raw);}
int64_t xl6TlsVerifiedPeerPathCountV1(void* raw)
{return (int64_t)xl6TlsVerifiedPeerPacketPathCountV1(raw);}
static void* certificate(void* raw,int64_t index,bool path)
{
    if(xrtGetError())return NULL;
    if(index<0||(uint64_t)index>SIZE_MAX){xl6TlsFailV1(XERR_RANGE,"verified TLS certificate index exceeds its native range");return NULL;}
    xbytesview der=path?xl6TlsVerifiedPeerPacketPathDerV1(raw,(size_t)index):
        xl6TlsVerifiedPeerPacketCertificateDerV1(raw,(size_t)index);
    return xrtGetError()?NULL:xl6X509CertificateCreateSpanV1(der);
}
void* xl6TlsVerifiedPeerCertificateAtV1(void* raw,int64_t index){return certificate(raw,index,false);}
void* xl6TlsVerifiedPeerPathAtV1(void* raw,int64_t index){return certificate(raw,index,true);}
void* xl6TlsVerifiedPeerAnchorV1(void* raw)
{
    xbytesview der=xl6TlsVerifiedPeerPacketAnchorDerV1(raw);
    return xrtGetError()?NULL:xl6X509CertificateCreateSpanV1(der);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsVerifiedPeerHandlePolicyV1Get,xl6TlsVerifiedPeerDestroyV1)
