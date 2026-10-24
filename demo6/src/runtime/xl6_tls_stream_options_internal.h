#ifndef XL6_TLS_STREAM_OPTIONS_INTERNAL_H
#define XL6_TLS_STREAM_OPTIONS_INTERNAL_H
#include "xl6_tls_internal.h"
/* Immutable by-value public configuration: no Engine, Session, callback,
 * language value or borrowed storage. Copy never starts a TLS/TCP service. */
void* xl6TlsStreamOptionsCopyV1(const xtlsstreamconfig*);
const XL6_NativeHandlePolicyV1* xl6TlsStreamOptionsHandlePolicyV1Get(void);
#endif
