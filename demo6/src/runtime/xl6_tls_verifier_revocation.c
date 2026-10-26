#include "xl6_tls_revocation_internal.h"
#include "xl6_tls_verifier_context.h"
void* xl6TlsVerifierCreateCallbackOwnerInternalV1(XL6_NativeClassResultContextV1,
    void*,void*,struct xlir_closure*,XL6_TlsPeerPolicyV1);
void* xl6TlsVerifierCreateRevocationV1(void* trust,void* policy,void* revocation)
{
    XL6_TlsPeerPolicyV1 owner=xl6TlsRevocationPolicyOwnerV1(revocation);
    return owner.data?xl6TlsVerifierCreatePoliciesInternalV1(trust,policy,(XL6_TlsPeerPolicyV1){0},owner):NULL;
}
void* xl6TlsVerifierCreateCallbackRevocationV1(XL6_NativeClassResultContextV1 context,
    void* trust,void* policy,struct xlir_closure* callback,void* revocation)
{
    XL6_TlsPeerPolicyV1 owner=xl6TlsRevocationPolicyOwnerV1(revocation);
    return owner.data?xl6TlsVerifierCreateCallbackOwnerInternalV1(context,trust,policy,callback,owner):NULL;
}
