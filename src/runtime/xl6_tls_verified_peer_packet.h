#ifndef XL6_TLS_VERIFIED_PEER_PACKET_H
#define XL6_TLS_VERIFIED_PEER_PACKET_H

#include "xl6_text_value.h"

struct xtlsverifiedpeer;
typedef struct XL6_TlsVerifiedPeerPacketV1 XL6_TlsVerifiedPeerPacketV1;

/* Private resident preparation for post-verification callbacks, not a public
 * constructor or an authentication operation. The producer must keep the
 * native peer and its views stable throughout Copy. The returned packet owns
 * its name, complete presented chain, selected path and anchor certificate.
 * It contains no language value, closure, generated address or native owner.
 * Role identifies the peer, not the local endpoint. Time is copied unchanged
 * from the native callback (milliseconds in xlang's pinned snapshot).
 */
XL6_TlsVerifiedPeerPacketV1* xl6TlsVerifiedPeerPacketCopyV1(const struct xtlsverifiedpeer*);
XL6_TlsVerifiedPeerPacketV1* xl6TlsVerifiedPeerPacketRefV1(XL6_TlsVerifiedPeerPacketV1*);
void xl6TlsVerifiedPeerPacketDropV1(XL6_TlsVerifiedPeerPacketV1*);

/* Internal read-only borrows, valid while the caller owns a packet reference.
 * A future language facade must copy/project them into owning language values.
 * These functions must never be registered as borrowed language results.
 */
int64_t xl6TlsVerifiedPeerPacketRoleV1(const XL6_TlsVerifiedPeerPacketV1*);
int64_t xl6TlsVerifiedPeerPacketTimeV1(const XL6_TlsVerifiedPeerPacketV1*);
xstrview xl6TlsVerifiedPeerPacketNameViewV1(const XL6_TlsVerifiedPeerPacketV1*);
size_t xl6TlsVerifiedPeerPacketCertificateCountV1(const XL6_TlsVerifiedPeerPacketV1*);
size_t xl6TlsVerifiedPeerPacketPathCountV1(const XL6_TlsVerifiedPeerPacketV1*);
xbytesview xl6TlsVerifiedPeerPacketCertificateDerV1(const XL6_TlsVerifiedPeerPacketV1*,size_t);
xbytesview xl6TlsVerifiedPeerPacketPathDerV1(const XL6_TlsVerifiedPeerPacketV1*,size_t);
xbytesview xl6TlsVerifiedPeerPacketAnchorDerV1(const XL6_TlsVerifiedPeerPacketV1*);

#endif
