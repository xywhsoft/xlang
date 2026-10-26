#ifndef XL6_TLS_VERIFIER_INTERNAL_H
#define XL6_TLS_VERIFIER_INTERNAL_H
#include "xl6_tls_internal.h"
/* Language-owned, signed budgets stay usable without selecting the native
 * verifier implementation just to make its feature-gated type visible. */
typedef struct XL6_TlsVerificationLimitsV1 {
#define XL6_TLS_VERIFY_LIMIT(Field,Name) int64_t Field;
#include "xl6_tls_verification_policy_fields.inc"
#undef XL6_TLS_VERIFY_LIMIT
} XL6_TlsVerificationLimitsV1;
typedef struct XL6_TlsVerificationPolicyV1 {
    XL6_TlsVerificationLimitsV1 limits;
    xtime at;
    bool fixedTime,allowSha1;
} XL6_TlsVerificationPolicyV1;
/* policy is borrowed from value's independently owned immutable Context.
 * Retaining value therefore also retains this metadata, including when a
 * native session outlives the language wrapper. The native Context owns the
 * complete optional policy capability; the facade never owns it separately. */
typedef struct XL6_TlsVerifierV1 {
    struct xtlsverifier* value;
    const XL6_TlsVerificationPolicyV1* policy;
} XL6_TlsVerifierV1;
bool xl6TlsVerificationPolicyInitV1(XL6_TlsVerificationPolicyV1*);
void* xl6TlsVerificationPolicyCopyV1(const XL6_TlsVerificationPolicyV1*);
void xl6TlsVerificationPolicyDestroyV1(void*);
void xl6TlsVerifierDestroyV1(void*);
XL6_TlsVerifierV1* xl6TlsVerifierCopyV1(const XL6_TlsVerifierV1*);
#endif
