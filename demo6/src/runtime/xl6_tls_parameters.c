#include "xl6_tls_parameters_internal.h"
void xl6TlsParametersDestroyV1(void* raw)
{
    xerror* prior=xrtTakeError();xrtTlsContextRelease(raw);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
void* xl6TlsParametersDefaultV1(void){return xrtGetError()?NULL:xrtTlsContextCreate(NULL);}
void* xl6TlsParametersCreateV1(void* policy,void* limits)
{
    if(xrtGetError())return NULL;
    if(!policy || !limits){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS policy or limits");return NULL;}
    xtlscontextconfig config={xrtTlsContextPolicy(policy),*(const xtlslimits*)limits};
    return config.Policy?xrtTlsContextCreate(&config):NULL;
}
void* xl6TlsParametersPolicyV1(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS parameters");return NULL;}
    return xrtTlsContextRetain(raw);
}
void* xl6TlsParametersLimitsV1(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS parameters");return NULL;}
    return xl6TlsLimitsCopyV1(xrtTlsContextLimits(raw));
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsParametersHandlePolicyV1Get,xl6TlsParametersDestroyV1)
