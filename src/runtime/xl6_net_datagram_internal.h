#ifndef XL6_NET_DATAGRAM_INTERNAL_H
#define XL6_NET_DATAGRAM_INTERNAL_H
#include "xl6_text_value.h"
/* POD snapshots: never retain a socket, Buffer, language value or address.
 * Info is caller-exclusive mutable output; Control is immutable after creation. */
typedef struct XL6_NetDatagramInfoV1 {
    xnetaddr remote;
    xnetdgrammeta meta;
    bool valid;
} XL6_NetDatagramInfoV1;
#endif
