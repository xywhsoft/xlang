#ifndef XL6_TLS_SELECTOR_INTERNAL_H
#define XL6_TLS_SELECTOR_INTERNAL_H
#include "xl6_tls_role_context_internal.h"
#include "xl6_tls_selector_abi.h"
/* Only native immutable identity material: no language/callback ownership. */
typedef struct XL6_TlsServerChoiceV1 {
    xtlsidentity* identity;
    uint64_t cookie;
    int64_t protocolIndex;
    bool hasProtocolIndex;
} XL6_TlsServerChoiceV1;
const XL6_NativeHandlePolicyV1* xl6TlsServerChoiceHandlePolicyV1Get(void);
const XL6_NativeHandlePolicyV1* xl6TlsServerRequestHandlePolicyV1Get(void);
#endif
