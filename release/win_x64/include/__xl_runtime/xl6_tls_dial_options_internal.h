#ifndef XL6_TLS_DIAL_OPTIONS_INTERNAL_H
#define XL6_TLS_DIAL_OPTIONS_INTERNAL_H
#include "xl6_tls_stream_options_internal.h"
#include "xl6_tcp_options_internal.h"
#include "xl6_text_internal.h"
#include "xl6_tls_dial_options_abi.h"
/* One immutable allocation owns both name views and their exact byte storage.
 * Native configuration is by-value, with no language or callback references. */
typedef struct XL6_TlsDialOptionsV1 {
    xtlsdialconfig native;
    xstrview serverName,verifyName;
    char names[];
} XL6_TlsDialOptionsV1;
void xl6TlsDialDefaultsV1(xtlsdialconfig*);
const XL6_NativeHandlePolicyV1* xl6TlsDialOptionsHandlePolicyV1Get(void);
#endif
