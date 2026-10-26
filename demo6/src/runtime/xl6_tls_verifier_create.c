#include "xl6_tls_verifier_internal.h"
#include "xl6_x509_store_internal.h"
#include "xl6_tls_verifier_context.h"
bool xl6TlsVerificationPolicyInitV1(XL6_TlsVerificationPolicyV1* policy)
{
    if(xrtGetError())return false;
    if(!policy)return xl6TlsFailV1(XERR_ARGUMENT,"missing TLS verification policy output");
    xtlsverifylimits limits;xrtTlsVerifyLimitsInit(&limits);*policy=(XL6_TlsVerificationPolicyV1){0};
#define XL6_TLS_VERIFY_LIMIT(Field,Name) if(limits.Field>INT64_MAX)return xl6TlsFailV1(XERR_RANGE,"native TLS budget exceeds language integer range");policy->limits.Field=(int64_t)limits.Field;
#include "xl6_tls_verification_policy_fields.inc"
#undef XL6_TLS_VERIFY_LIMIT
    return true;
}
void* xl6TlsVerifierCreatePeerPolicyInternalV1(void* rawTrust,const XL6_TlsVerificationPolicyV1* policy,XL6_TlsPeerPolicyV1 callback)
{
    if(xrtGetError())return NULL;
    const XL6_X509StoreVersionV1* trust=rawTrust;
    if(!trust || !trust->value || !policy){xl6TlsFailV1(XERR_ARGUMENT,"TLS verifier needs an immutable trust snapshot and policy");return NULL;}
    size_t count=xrtX509StoreCount(trust->value);
    if(!count){xl6TlsFailV1(XERR_VALUE,"TLS verifier requires at least one trust anchor");return NULL;}
    if(trust->bytes<0){xl6TlsFailV1(XERR_STATE,"TLS trust snapshot has invalid byte accounting");return NULL;}
    if((uint64_t)trust->bytes>SIZE_MAX){xl6TlsFailV1(XERR_RANGE,"TLS trust bytes exceed native size range");return NULL;}
    if(!xl6TlsLimitV1(count,(int64_t)policy->limits.MaxTrustCertificates,"TLS trust certificate count exceeds its budget") ||
        !xl6TlsLimitV1((size_t)trust->bytes,(int64_t)policy->limits.MaxTrustBytes,"TLS trust bytes exceed their budget"))return NULL;
    XL6_TlsVerifierContextV1* context=xl6TlsVerifierContextCreateV1(policy,callback);if(!context)return NULL;
    xtlsverifylimits limits;
#define XL6_TLS_VERIFY_LIMIT(Field,Name) limits.Field=(size_t)policy->limits.Field;
#include "xl6_tls_verification_policy_fields.inc"
#undef XL6_TLS_VERIFY_LIMIT
    xtlsverifierconfig config;xrtTlsVerifierConfigInit(&config);config.Store=trust->value;
    const xtlsverifiercontextownershipv1* ownership=xl6TlsVerifierContextNativePolicyV1Get();
    config.Context=context;config.Release=ownership->Drop;config.AllowSha1=policy->allowSha1;
    config.Time=ownership->Time;config.Policy=ownership->Policy;
    xtlsverifier* value=xrtTlsVerifierCreateOwnedV1(&config,&limits,ownership);
    if(!value){xerror* error=xrtTakeError();xl6TlsVerifierContextDropV1(context);xrtSetErrorTake(error);return NULL;}
    XL6_TlsVerifierV1* self=xrtMalloc(sizeof(*self));
    if(!self){xerror* error=xrtTakeError();xrtTlsVerifierRelease(value);xrtSetErrorTake(error);return NULL;}
    *self=(XL6_TlsVerifierV1){value,xl6TlsVerifierContextPolicyV1(context)};return self;
}
void* xl6TlsVerifierCreateV1(void* trust)
{XL6_TlsVerificationPolicyV1 policy;if(!xl6TlsVerificationPolicyInitV1(&policy))return NULL;return xl6TlsVerifierCreatePeerPolicyInternalV1(trust,&policy,(XL6_TlsPeerPolicyV1){0});}
void* xl6TlsVerifierCreatePolicyV1(void* trust,void* policy)
{return xl6TlsVerifierCreatePeerPolicyInternalV1(trust,policy,(XL6_TlsPeerPolicyV1){0});}
