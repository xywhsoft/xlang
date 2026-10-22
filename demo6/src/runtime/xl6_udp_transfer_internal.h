#ifndef XL6_UDP_TRANSFER_INTERNAL_H
#define XL6_UDP_TRANSFER_INTERNAL_H
#include "xl6_udp_internal.h"
#include "xl6_native_class_borrow.h"
#include "xl6_net_batch_progress_internal.h"
typedef bool (*XL6_UdpBatchFillV1)(void*,void*);
/* Permanent call-only descriptor projection; no callback/context is retained
 * by native sends. Public wrappers fix both policy and physical descriptor. */
XL6_ProductI64U64V1 xl6UdpBatchSendInternalV1(XL6_NativeClassArrayContextV1,void*,const xvalue*,void*,int64_t,int64_t,bool,const XL6_NativeHandlePolicyV1*,XL6_UdpBatchFillV1);
#endif
