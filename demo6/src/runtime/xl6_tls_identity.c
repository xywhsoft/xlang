#include "xl6_tls_identity_internal.h"

void xl6TlsIdentityDestroyV1(void* raw)
{
    xerror* prior=xrtTakeError();xrtTlsIdentityRelease(raw);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
int64_t xl6TlsIdentityKindV1(void* raw)
{
    if(xrtGetError())return 0;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS identity");return 0;}
    return (int64_t)xrtTlsIdentityType(raw);
}
int64_t xl6TlsIdentityCountV1(void* raw)
{
    if(xrtGetError())return 0;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS identity");return 0;}
    size_t count=xrtTlsIdentityCertificateCount(raw);
    if(count>INT64_MAX){xl6TlsFailV1(XERR_RANGE,"TLS certificate count exceeds language range");return 0;}
    return (int64_t)count;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsIdentityHandlePolicyV1Get,xl6TlsIdentityDestroyV1)
