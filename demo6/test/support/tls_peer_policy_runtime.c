/* Normal fixed trust, real closure/code leases, owning callback arguments and
 * checked predicate outcomes. No sockets, malformed crypto or security tests. */
#include "src/runtime/xl6_tls_verifier_context.h"
#include "src/runtime/xl6_tls_verified_peer_packet.h"
#include "src/runtime/xl6_native_class_callback.h"
#include "src/runtime/xl6_x509_store_internal.h"
#include "src/runtime/xl6_x509_internal.h"
#include <assert.h>
#include <stdio.h>
#include "tls_verifier_vectors.h"

typedef struct Owner {unsigned pins,acquired,released;bool closed;} Owner;
static int acquire(void* raw){Owner* p=raw;if(p->closed)return 0;p->pins++;p->acquired++;return 1;}
static void release(void* raw){Owner* p=raw;assert(p->pins);p->pins--;p->released++;}
typedef struct State {unsigned calls,mode;xvalue* saved;} State;
typedef struct Environment {State* state;} Environment;
static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_TRIVIAL,0,sizeof(State*)};
static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(Environment),1,&slot};
static const XL6_ClassIdentitySchemaV3 schema={sizeof(schema),UINT64_C(0x746c735665726966),1701,
    XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}};
static bool callback(void* raw,bool* output,xvalue* input)
{
    State* state=((Environment*)raw)->state;state->calls++;assert(!state->saved);
    state->saved=xrtValueRetain(input);assert(state->saved&&xrtValueTypeId(input)==schema.identity);
    xvalue* handle=xrtValueObjectGet(input,(xstrview){"payload",7});void* packet=NULL;
    assert(handle&&xrtValueGetHandle(handle,&packet,NULL,NULL)&&packet);
    assert(xl6TlsVerifiedPeerRoleV1(packet)==XTLS_SERVER&&xl6TlsVerifiedPeerTimeV1(packet)==pathTime&&
        xl6TlsVerifiedPeerCertificateCountV1(packet)==2&&xl6TlsVerifiedPeerPathCountV1(packet)==2);
    xstrview name=xl6TlsVerifiedPeerNameV1(packet);
    if(!name.Data)return false;
    assert(name.Size==9&&!memcmp(name.Data,"localhost",9));xrtFree((void*)name.Data);
    void* anchor=xl6TlsVerifiedPeerAnchorV1(packet);if(!anchor)return false;xl6X509CertificateDestroyV1(anchor);
    if(state->mode==2)return false;
    if(state->mode==3){xrtSetErrorKind(XERR_RANGE);return false;}
    if(state->mode==4){xrtSetErrorKind(XERR_RANGE);*output=true;return true;}
    *output=state->mode==0;return true;
}
static XL6_Closure* target(const XL6_CodeOwner* owner,State* state)
{
    Environment* env=xrtMalloc(sizeof(*env));assert(env);env->state=state;
    XL6_Closure* result=xl6ClosureRuntimeV2Get()->Create(owner,(void(*)(void))callback,NULL,env,&layout,NULL,NULL,0);
    assert(result);return result;
}
static XL6_NativeClassResultContextV1 context(const XL6_CodeOwner* owner)
{return (XL6_NativeClassResultContextV1){{"payload",7,xl6TlsVerifiedPeerHandlePolicyV1Get()},owner,&schema};}
static XL6_TlsVerificationPolicyV1 policy(void)
{XL6_TlsVerificationPolicyV1 p;assert(xl6TlsVerificationPolicyInitV1(&p));p.fixedTime=true;p.at=pathTime;return p;}
static void* trust(void)
{
    xx509store* store=xrtX509StoreCreate();assert(store&&xrtX509StoreAdd(store,path_rsa_root,sizeof(path_rsa_root))==X509_VALUE);
    void* result=xl6X509StoreVersionTakeV1(&store,sizeof(path_rsa_root));assert(result&&!store);return result;
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
}
static void outcomes(unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Owner a={0},b={0};XL6_CodeOwner ownerA={sizeof(ownerA),&a,acquire,release},ownerB={sizeof(ownerB),&b,acquire,release};
    State state={.mode=mode};XL6_Closure* original=target(&ownerB,&state);void* source=trust();XL6_TlsVerificationPolicyV1 p=policy();
    XL6_TlsVerifierV1* verifier=xl6TlsVerifierCreateCallbackV1(context(&ownerA),source,&p,original);assert(verifier);
    xl6X509StoreSnapshotDestroyV1(source);xl6ClosureRuntimeV1Get()->Release(original);a.closed=b.closed=true;
    assert(a.pins==1&&b.pins==1);xx509cert views[2];
    assert(xrtX509Parse(path_rsa_leaf,sizeof(path_rsa_leaf),views)&&xrtX509Parse(path_rsa_mid,sizeof(path_rsa_mid),views+1));
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
    bool ok=xrtTlsVerifierVerify(verifier->value,XTLS_SERVER,(xstrview){"localhost",9},views,2);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);assert(ok==(mode==0)&&result.ok==(mode==0));
    assert(state.calls==1&&state.saved);
    if(mode==1)assert(xrtErrorKind(result.error)==XERR_PERMISSION);
    if(mode==2)assert(!result.error);
    if(mode==3||mode==4)assert(xrtErrorKind(result.error)==XERR_RANGE);
    xrtErrorFree(result.error);xl6TlsVerifierDestroyV1(verifier);assert(a.pins==1&&!b.pins);
    void* packet=NULL;assert(xrtValueGetHandle(xrtValueObjectGet(state.saved,(xstrview){"payload",7}),&packet,NULL,NULL));
    void* detached=xl6TlsVerifiedPeerCertificateAtV1(packet,0);assert(detached);
    xrtValueRelease(state.saved);assert(!a.pins&&a.released==1&&b.released==1);
    const xx509cert* leaf=&((const XL6_X509CertificateV1*)detached)->view;
    assert(leaf->Raw.Size==sizeof(path_rsa_leaf)&&!memcmp(leaf->Raw.Data,path_rsa_leaf,leaf->Raw.Size));
    xl6X509CertificateDestroyV1(detached);assert(!xrtGetError());balanced(before);
}
static void prefix(bool invoke,size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Owner a={0},b={0};XL6_CodeOwner ownerA={sizeof(ownerA),&a,acquire,release},ownerB={sizeof(ownerB),&b,acquire,release};
    State state={0};XL6_Closure* original=target(&ownerB,&state);void* source=trust();XL6_TlsVerificationPolicyV1 p=policy();
    XL6_TlsVerifierV1* verifier=NULL;
    if(invoke){verifier=xl6TlsVerifierCreateCallbackV1(context(&ownerA),source,&p,original);assert(verifier);}
    xx509cert views[2];assert(xrtX509Parse(path_rsa_leaf,sizeof(path_rsa_leaf),views)&&xrtX509Parse(path_rsa_mid,sizeof(path_rsa_mid),views+1));
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));assert(xrtMemDebugFailAfter(point));bool ok=false;
    if(invoke)ok=xrtTlsVerifierVerify(verifier->value,XTLS_SERVER,(xstrview){"localhost",9},views,2);
    else verifier=xl6TlsVerifierCreateCallbackV1(context(&ownerA),source,&p,original);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result result=xl6ErrorScopeEnd(&errors);
    if(!point)assert(hit);
    if(hit)assert(invoke?!ok:!verifier);else assert(invoke?ok:verifier!=NULL);
    xrtErrorFree(result.error);xrtClearError();xl6TlsVerifierDestroyV1(verifier);xrtValueRelease(state.saved);
    xl6ClosureRuntimeV1Get()->Release(original);xl6X509StoreSnapshotDestroyV1(source);
    assert(!a.pins&&!b.pins&&a.acquired==a.released&&b.acquired==b.released&&!xrtGetError());balanced(before);
}
int main(void)
{
    for(unsigned mode=0;mode<5;mode++)outcomes(mode);
    for(unsigned invoke=0;invoke<2;invoke++)for(size_t point=0;point<96;point++)prefix(invoke!=0,point);
    puts("TLS peer policy: default trust, bool predicates, owning peer/certificates, independent Close leases, nil/first failures, 192 OOM prefixes; memory balanced");return 0;
}
