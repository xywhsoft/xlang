#ifndef XL6_NET_DATAGRAM_ERROR_INTERNAL_H
#define XL6_NET_DATAGRAM_ERROR_INTERNAL_H
#include "xl6_text_value.h"
/* Reusable caller-exclusive output. No retained language/native references. */
typedef struct XL6_NetDatagramErrorInfoV1 {
    xnetdgramerror error;
    bool valid;
} XL6_NetDatagramErrorInfoV1;
#endif
