#ifndef XL6_UDP_MESSAGE_INTERNAL_H
#define XL6_UDP_MESSAGE_INTERNAL_H
#include "xl6_udp_internal.h"
#include "xl6_native_class_borrow.h"
/* Immutable, uniquely contained metadata. Each payload slot is a REAL native
 * owner, independent of source language wrappers, Socket, Source and Engine. */
typedef struct XL6_UdpMessageV1 {
    xnetudpownedmessage native;
    xnetaddr remote;
    xnetdgramcontrol control;
    uint64_t length;
} XL6_UdpMessageV1;
XL6_UdpMessageV1* xl6UdpMessageAllocateInternalV1(size_t);
XL6_UdpMessageV1* xl6UdpMessageDiscardInternalV1(XL6_UdpMessageV1*);
bool xl6UdpMessageSizeValidInternalV1(size_t);
XL6_UdpMessageV1* xl6UdpMessageCopyInternalV1(xbytesview,int64_t);
#endif
