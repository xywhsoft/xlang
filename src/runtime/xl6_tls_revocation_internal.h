#ifndef XL6_TLS_REVOCATION_INTERNAL_H
#define XL6_TLS_REVOCATION_INTERNAL_H
#include "xl6_tls_verified_peer_packet.h"
#include "xl6_tls_peer_policy_owner.h"
#include "xl6_x509_crl_policy_internal.h"
typedef struct XL6_TlsRevocationOptionsV1 {
    bool allowUnknown;
    int64_t skew;
    uint32 flags;
    size_t maxSources,maxSignerCertificates;
} XL6_TlsRevocationOptionsV1;
typedef struct XL6_TlsRevocationRequestV1 {
    XL6_TlsVerifiedPeerPacketV1* peer;
    size_t index;
    XL6_TlsRevocationOptionsV1 options;
} XL6_TlsRevocationRequestV1;
XL6_TlsRevocationRequestV1* xl6TlsRevocationRequestCreateV1(
    XL6_TlsVerifiedPeerPacketV1*,size_t,const XL6_TlsRevocationOptionsV1*);
bool xl6TlsRevocationCheckBoundV1(const XL6_TlsRevocationRequestV1*,const XL6_X509RevocationCheckV1*);
XL6_TlsPeerPolicyV1 xl6TlsRevocationPolicyOwnerV1(void*);
#endif
