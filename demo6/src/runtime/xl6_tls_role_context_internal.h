#ifndef XL6_TLS_ROLE_CONTEXT_INTERNAL_H
#define XL6_TLS_ROLE_CONTEXT_INTERNAL_H
#include "xl6_tls_verifier_internal.h"
#include "xl6_tls_selector_owner.h"
/* One allocation owns the ordered native views and their exact opaque bytes.
 * Empty protocol lists use NULL. No source Array, xvalue or callback survives. */
typedef struct XL6_TlsProtocolsV1 {
    size_t count,bytes;
    xstrview views[];
} XL6_TlsProtocolsV1;
typedef struct XL6_TlsClientContextV1 {
    struct xtlscontext* parameters;
    XL6_TlsVerifierV1 verifier;
    XL6_TlsProtocolsV1* protocols;
} XL6_TlsClientContextV1;
typedef struct XL6_TlsServerContextV1 {
    struct xtlscontext* parameters;
    struct xtlsidentity* identity;
    XL6_TlsProtocolsV1* protocols;
    bool requireProtocol;
    XL6_TlsSelectorV1 selector;
} XL6_TlsServerContextV1;
bool xl6TlsProtocolsCopyV1(const XL6_TlsProtocolsV1*,XL6_TlsProtocolsV1**);
bool xl6TlsProtocolsReadV1(const xvalue*,XL6_TlsProtocolsV1**);
xvalue* xl6TlsProtocolsAtV1(const XL6_TlsProtocolsV1*,int64_t);
/* Internal factories consume the protocol packet on every path. All native
 * material and parameters are borrowed only until independent retains finish. */
void* xl6TlsClientContextFromV1(struct xtlscontext*,const XL6_TlsVerifierV1*,XL6_TlsProtocolsV1*);
void* xl6TlsServerContextFromV1(struct xtlscontext*,struct xtlsidentity*,XL6_TlsProtocolsV1*,bool);
void* xl6TlsServerContextFromSelectorV1(struct xtlscontext*,struct xtlsidentity*,XL6_TlsProtocolsV1*,bool,XL6_TlsSelectorV1);
#endif
