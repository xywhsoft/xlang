#ifndef XL6_TLS_VERIFIER_CONTEXT_H
#define XL6_TLS_VERIFIER_CONTEXT_H
#include "xl6_tls_verifier_internal.h"
#include "xl6_tls_peer_policy_owner.h"
/* The native Verifier owns exactly one counted Context. All metadata and the
 * optional actual policy owner stay alive even after every facade disappears.
 * No facade pointer, trust snapshot wrapper or generated address is borrowed. */
typedef struct XL6_TlsVerifierContextV1 XL6_TlsVerifierContextV1;
XL6_TlsVerifierContextV1* xl6TlsVerifierContextCreateV1(
    const XL6_TlsVerificationPolicyV1*,XL6_TlsPeerPolicyV1);
const XL6_TlsVerificationPolicyV1* xl6TlsVerifierContextPolicyV1(const XL6_TlsVerifierContextV1*);
void xl6TlsVerifierContextDropV1(void*);
const xtlsverifiercontextownershipv1* xl6TlsVerifierContextNativePolicyV1Get(void);
const xrtownershipquiescencev1* xl6TlsVerifierContextQuiescenceV1Get(void);
const xrtownershipadapterv1* xl6TlsVerifierContextOwnershipAdapterV1(xrtownershipref);
void* xl6TlsVerifierCreatePeerPolicyInternalV1(void*,const XL6_TlsVerificationPolicyV1*,XL6_TlsPeerPolicyV1);
#endif
