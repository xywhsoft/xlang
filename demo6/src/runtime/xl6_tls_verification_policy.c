#include "xl6_tls_verifier_internal.h"
void* xl6TlsVerificationPolicyCopyV1(const XL6_TlsVerificationPolicyV1* policy)
{
    if(xrtGetError())return NULL;
    if(!policy){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS verification policy");return NULL;}
#define XL6_TLS_VERIFY_LIMIT(Field,Name) if(policy->limits.Field<0 || (uint64_t)policy->limits.Field>SIZE_MAX){xl6TlsFailV1(XERR_RANGE,"TLS verification budget exceeds its native range");return NULL;}
#include "xl6_tls_verification_policy_fields.inc"
#undef XL6_TLS_VERIFY_LIMIT
    XL6_TlsVerificationPolicyV1* copy=xrtMalloc(sizeof(*copy));if(copy)*copy=*policy;return copy;
}
void xl6TlsVerificationPolicyDestroyV1(void* raw){xrtFree(raw);}
void* xl6TlsVerificationPolicyCreateV1(int64_t count,int64_t bytes,int64_t path,int64_t steps,int64_t trustCount,int64_t trustBytes,bool allowSha1)
{
    if(xrtGetError())return NULL;
    if(count<0 || bytes<0 || path<0 || steps<0 || trustCount<0 || trustBytes<0 ||
        (uint64_t)count>SIZE_MAX || (uint64_t)bytes>SIZE_MAX || (uint64_t)path>SIZE_MAX ||
        (uint64_t)steps>SIZE_MAX || (uint64_t)trustCount>SIZE_MAX || (uint64_t)trustBytes>SIZE_MAX){
        xl6TlsFailV1(XERR_RANGE,"TLS verification budget exceeds its native range");return NULL;
    }
    XL6_TlsVerificationPolicyV1 value={.limits={count,bytes,path,steps,trustCount,trustBytes},.allowSha1=allowSha1};
    return xl6TlsVerificationPolicyCopyV1(&value);
}
static const XL6_TlsVerificationPolicyV1* checked(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw)xl6TlsFailV1(XERR_ARGUMENT,"missing TLS verification policy");
    return raw;
}
#define XL6_TLS_VERIFY_LIMIT(Field,Name) int64_t xl6TlsVerificationPolicy##Field##V1(void* raw){const XL6_TlsVerificationPolicyV1* p=checked(raw);return p?(int64_t)p->limits.Field:0;}
#include "xl6_tls_verification_policy_fields.inc"
#undef XL6_TLS_VERIFY_LIMIT
bool xl6TlsVerificationPolicyAllowsSha1V1(void* raw)
{const XL6_TlsVerificationPolicyV1* p=checked(raw);return p && p->allowSha1;}
bool xl6TlsVerificationPolicyUsesCurrentTimeV1(void* raw)
{const XL6_TlsVerificationPolicyV1* p=checked(raw);return p && !p->fixedTime;}
int64_t xl6TlsVerificationPolicyFixedTimeV1(void* raw)
{
    const XL6_TlsVerificationPolicyV1* p=checked(raw);if(!p)return 0;
    if(!p->fixedTime){xl6TlsFailV1(XERR_STATE,"TLS policy uses the current clock, not a fixed time");return 0;}
    return p->at;
}
void* xl6TlsVerificationPolicyWithTimeV1(void* raw,int64_t at)
{
    const XL6_TlsVerificationPolicyV1* p=checked(raw);if(!p)return NULL;
    XL6_TlsVerificationPolicyV1 value=*p;value.fixedTime=true;value.at=at;
    return xl6TlsVerificationPolicyCopyV1(&value);
}
void* xl6TlsVerificationPolicyWithCurrentTimeV1(void* raw)
{
    const XL6_TlsVerificationPolicyV1* p=checked(raw);if(!p)return NULL;
    XL6_TlsVerificationPolicyV1 value=*p;value.fixedTime=false;value.at=0;
    return xl6TlsVerificationPolicyCopyV1(&value);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsVerificationPolicyHandlePolicyV1Get,xl6TlsVerificationPolicyDestroyV1)
