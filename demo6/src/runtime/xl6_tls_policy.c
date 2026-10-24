#include "xl6_tls_parameters_internal.h"
void xl6TlsPolicyDestroyV1(void* raw)
{
    xerror* prior=xrtTakeError();xrtTlsContextRelease(raw);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
void* xl6TlsPolicyDefaultV1(void){return xrtGetError()?NULL:xrtTlsContextCreate(NULL);}
static const xtlspolicy* checked(void* raw)
{
    if(xrtGetError())return NULL;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS policy");return NULL;}
    return xrtTlsContextPolicy(raw);
}
#define XL6_TLS_POLICY_LIST(Singular,Plural,Type,Limit) \
int64_t xl6TlsPolicy##Singular##CountV1(void* raw){const xtlspolicy* p=checked(raw);return p?(int64_t)p->Singular##Count:0;} \
int64_t xl6TlsPolicy##Singular##AtV1(void* raw,int64_t index){const xtlspolicy* p=checked(raw);if(!p)return 0; \
if(index<0 || (uint64_t)index>=p->Singular##Count){xl6TlsFailV1(XERR_RANGE,"TLS policy index is out of range");return 0;}return p->Plural[index];}
#include "xl6_tls_policy_lists.inc"
#undef XL6_TLS_POLICY_LIST
int64_t xl6TlsPolicyKeyShareV1(void* raw){const xtlspolicy* p=checked(raw);return p?p->KeySharePolicy:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsPolicyHandlePolicyV1Get,xl6TlsPolicyDestroyV1)
