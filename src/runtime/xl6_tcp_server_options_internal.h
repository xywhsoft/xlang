#ifndef XL6_TCP_SERVER_OPTIONS_INTERNAL_H
#define XL6_TCP_SERVER_OPTIONS_INTERNAL_H
#include "xl6_tcp_options_internal.h"
/* One independent immutable allocation. Additional is rebased to THIS owner,
 * never to another options object, caller array, address or language record. */
typedef struct XL6_TcpServerOptionsV1 {
    xnetserverconfig config;
    xnetlistenconfig additional[];
} XL6_TcpServerOptionsV1;
static inline const xnetserverconfig* xl6TcpServerOptionsNativeV1(const void* raw)
{return raw?&((const XL6_TcpServerOptionsV1*)raw)->config:NULL;}
#endif
