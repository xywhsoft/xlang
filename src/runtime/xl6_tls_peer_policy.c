#include "xl6_tls_verifier_context.h"
#include "xl6_tls_verified_peer_packet.h"
#include "xl6_native_class_callback.h"

static void* ref(void* raw){return xl6ClosureRuntimeV1Get()->Retain(raw);}
static void drop(void* raw){xl6ClosureRuntimeV1Get()->Release(raw);}
static xrtownershipref ownership(const void* raw){return xl6ClosureRuntimeV1Get()->Ownership(raw);}
static bool verify(void* raw,const xtlsverifiedpeer* peer)
{
    void* input=xl6TlsVerifiedPeerPacketCopyV1(peer);bool accepted=false;
    bool ok=input&&xl6NativeClassPredicateInvokeTakeV1(raw,&input,&accepted);
    if(input)xl6TlsVerifiedPeerDestroyV1(input);
    if(!ok){xl6ForeignCallbackFail();return false;}
    return accepted;
}
static const XL6_TlsPeerPolicyOpsV1 operations={ref,drop,ownership,verify};
void* xl6TlsVerifierCreateCallbackOwnerInternalV1(XL6_NativeClassResultContextV1 context,
    void* trust,void* policy,XL6_Closure* callback,XL6_TlsPeerPolicyV1 revocation)
{
    if(xrtGetError())return NULL;
    if(context.field.policy!=xl6TlsVerifiedPeerHandlePolicyV1Get()){
        xl6TlsFailV1(XERR_TYPE,"TLS predicate needs the sealed TlsVerifiedPeer recipe");return NULL;}
    XL6_Closure* driver=xl6NativeClassPredicateCreateV1(context,callback);if(!driver)return NULL;
    void* result=xl6TlsVerifierCreatePoliciesInternalV1(trust,policy,(XL6_TlsPeerPolicyV1){driver,&operations},revocation);
    drop(driver);return result;
}
void* xl6TlsVerifierCreateCallbackV1(XL6_NativeClassResultContextV1 context,
    void* trust,void* policy,XL6_Closure* callback)
{return xl6TlsVerifierCreateCallbackOwnerInternalV1(context,trust,policy,callback,(XL6_TlsPeerPolicyV1){0});}
