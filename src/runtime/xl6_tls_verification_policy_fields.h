#ifndef XL6_TLS_VERIFICATION_POLICY_FIELDS_H
#define XL6_TLS_VERIFICATION_POLICY_FIELDS_H
/* Shared declaration-only schema. Define XL6_TLS_VERIFY_LIMIT at the use
 * site, then expand as a layout, declaration, contract or getter list. */
#define XL6_TLS_VERIFICATION_POLICY_FIELDS() \
XL6_TLS_VERIFY_LIMIT(MaxPeerCertificates,maxPeerCertificates) \
XL6_TLS_VERIFY_LIMIT(MaxPeerBytes,maxPeerBytes) \
XL6_TLS_VERIFY_LIMIT(MaxPathCertificates,maxPathCertificates) \
XL6_TLS_VERIFY_LIMIT(MaxSearchSteps,maxSearchSteps) \
XL6_TLS_VERIFY_LIMIT(MaxTrustCertificates,maxTrustCertificates) \
XL6_TLS_VERIFY_LIMIT(MaxTrustBytes,maxTrustBytes)
#endif
