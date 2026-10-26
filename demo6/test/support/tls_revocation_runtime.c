/* Ordinary offline verification, actual typed callbacks, independent CodeLease
 * owners, immutable applicable evidence and allocation rollback. No sockets. */
#include "src/runtime/xl6_tls_revocation_internal.h"
#include "src/runtime/xl6_tls_verifier_context.h"
#include "src/runtime/xl6_native_class_callback.h"
#include "src/runtime/xl6_x509_store_internal.h"
#include <assert.h>
#include <stdio.h>
#include "tls_revocation_vectors.h"

typedef struct Owner {unsigned pins,acquired,released;bool closed;} Owner;
static int acquire(void* raw)
{Owner* p=raw;if(p->closed)return 0;p->pins++;p->acquired++;return 1;}
static void release(void* raw)
{Owner* p=raw;assert(p->pins);p->pins--;p->released++;}
typedef struct State {void* crls[2];unsigned calls,mode;xvalue* saved[2];} State;
typedef struct Environment {State* state;} Environment;
static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_TRIVIAL,0,sizeof(State*)};
static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(Environment),1,&slot};
static const XL6_ClassIdentitySchemaV3 requestSchema={sizeof(requestSchema),UINT64_C(0x63726c5265717565),1801,
    XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}};
static const XL6_ClassIdentitySchemaV3 checkSchema={sizeof(checkSchema),UINT64_C(0x63726c436865636b),1802,
    XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}};
static XL6_NativeClassCallbackContextV1 context(const XL6_CodeOwner* owner)
{
    return (XL6_NativeClassCallbackContextV1){
        {{"payload",7,xl6TlsRevocationRequestHandlePolicyV1Get()},owner,&requestSchema},
        {"payload",7,xl6X509RevocationCheckHandlePolicyV1Get()},checkSchema.identity};
}
static bool callback(void* raw,xvalue** output,xvalue* argument)
{
    State* state=((Environment*)raw)->state;unsigned index=state->calls++;assert(index<2&&!state->saved[index]);
    state->saved[index]=xrtValueRetain(argument);if(!state->saved[index])return false;
    if(state->mode==1||state->mode==4)return true;
    if(state->mode==2)return false;
    if(state->mode==3){xrtSetErrorKind(XERR_RANGE);return false;}
    void* request=NULL;assert(xrtValueGetHandle(xrtValueObjectGet(argument,(xstrview){"payload",7}),&request,NULL,NULL));
    assert(xl6TlsRevocationRequestIndexV1(request)==index&&xl6TlsRevocationRequestTimeV1(request)==pathTime&&
        xl6TlsRevocationRequestSkewV1(request)==0&&xl6TlsRevocationRequestFlagsV1(request)==31);
    void* target=xl6TlsRevocationRequestTargetV1(request);if(!target)return false;
    void* first=xl6X509RevocationCheckCreateV1(target);xl6X509CertificateDestroyV1(target);if(!first)return false;
    void* check=xl6X509RevocationCheckWithCrlV1(first,state->crls[index]);xl6X509RevocationCheckDestroyV1(first);
    if(!check)return false;
    XL6_NativeClassResultContextV1 result={{"payload",7,xl6X509RevocationCheckHandlePolicyV1Get()},NULL,&checkSchema};
    xvalue* shell=xl6NativeClassResultPrepareV1(result);
    *output=xl6NativeClassResultCommitTakeV1(result,&shell,&check);
    return *output!=NULL;
}
static XL6_Closure* target(const XL6_CodeOwner* owner,State* state)
{
    Environment* env=xrtMalloc(sizeof(*env));assert(env);env->state=state;
    XL6_Closure* value=xl6ClosureRuntimeV2Get()->Create(owner,(void(*)(void))callback,NULL,env,&layout,NULL,NULL,0);
    assert(value);return value;
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
}
typedef struct Material {void* trust;XL6_TlsVerificationPolicyV1 policy;xx509cert chain[2];} Material;
static Material material(size_t vector,State* state)
{
    const PathVector* input=pathVectors+vector;Material result={0};
    xx509store* store=xrtX509StoreCreate();assert(store&&xrtX509StoreAdd(store,input->root.Data,input->root.Size)==X509_VALUE);
    result.trust=xl6X509StoreVersionTakeV1(&store,input->root.Size);assert(result.trust&&!store);
    assert(xl6TlsVerificationPolicyInitV1(&result.policy));result.policy.fixedTime=true;result.policy.at=pathTime;
    assert(xrtX509Parse(input->leaf.Data,input->leaf.Size,result.chain)&&xrtX509Parse(input->mid.Data,input->mid.Size,result.chain+1));
    for(size_t i=0;i<2;i++){
        void* issuer=xl6X509CertificateCreateSpanV1(i?input->root:input->mid);assert(issuer);
        void* crl=xl6X509CrlCreateSpanV1(revocationCrls[vector][i]);assert(crl);
        state->crls[i]=xl6X509CrlValidateV1(crl,issuer,pathTime,0,31);assert(state->crls[i]);
        xl6X509CrlDestroyV1(crl);xl6X509CertificateDestroyV1(issuer);
    }
    return result;
}
static void dropState(State* state)
{
    for(size_t i=0;i<2;i++){xrtValueRelease(state->saved[i]);xl6X509ValidatedCrlDestroyV1(state->crls[i]);}
}
static void operation(size_t vector,unsigned mode,bool invoking,size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Owner a={0},b={0};XL6_CodeOwner ownerA={sizeof(ownerA),&a,acquire,release},ownerB={sizeof(ownerB),&b,acquire,release};
    State state={.mode=mode};Material source=material(vector,&state);XL6_Closure* original=target(&ownerB,&state);
    void* policy=NULL;XL6_TlsVerifierV1* verifier=NULL;
    bool fault=point!=SIZE_MAX;
    if(invoking){
        policy=xl6TlsRevocationPolicyCreateV1(context(&ownerA),original,mode==4,0,31,64,64);assert(policy);
        verifier=xl6TlsVerifierCreateRevocationV1(source.trust,&source.policy,policy);assert(verifier);
        xl6TlsRevocationPolicyDestroyV1(policy);policy=NULL;
        xl6ClosureRuntimeV1Get()->Release(original);original=NULL;a.closed=b.closed=true;
        assert(a.pins==1&&b.pins==1);
    }
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
    if(fault)assert(xrtMemDebugFailAfter(point));
    bool ok;
    if(invoking)ok=xrtTlsVerifierVerify(verifier->value,XTLS_SERVER,(xstrview){"localhost",9},source.chain,2);
    else{policy=xl6TlsRevocationPolicyCreateV1(context(&ownerA),original,false,0,31,64,64);ok=policy!=NULL;}
    bool hit=fault&&xrtMemDebugFailTriggered();if(fault)xrtMemDebugFailClear();
    xl6_error_result result=xl6ErrorScopeEnd(&errors);
    if(hit)assert(!ok&&!result.ok);
    else if(invoking){
        assert(ok==(mode==0||mode==4)&&result.ok==ok);
        assert(state.calls==(mode==0||mode==4?2:1));
        if(mode==1)assert(xrtErrorKind(result.error)==XERR_PERMISSION);
        if(mode==2)assert(!result.error);
        if(mode==3)assert(xrtErrorKind(result.error)==XERR_RANGE);
    }else assert(ok&&result.ok);
    xrtErrorFree(result.error);xl6TlsVerifierDestroyV1(verifier);
    xl6TlsRevocationPolicyDestroyV1(policy);xl6ClosureRuntimeV1Get()->Release(original);
    xl6X509StoreSnapshotDestroyV1(source.trust);
    /* Held owning requests remain valid after every verifier/policy wrapper. */
    for(size_t i=0;i<2;i++)if(state.saved[i]){
        void* request=NULL;assert(xrtValueGetHandle(xrtValueObjectGet(state.saved[i],(xstrview){"payload",7}),&request,NULL,NULL));
        assert(xl6TlsRevocationRequestIndexV1(request)==(int64_t)i);
        void* certificate=xl6TlsRevocationRequestIssuerV1(request);assert(certificate);xl6X509CertificateDestroyV1(certificate);
    }
    dropState(&state);assert(!a.pins&&!b.pins&&a.acquired==a.released&&b.acquired==b.released&&!xrtGetError());balanced(before);
}
static bool additional(void* environment,bool* output,xvalue* peer)
{(void)environment;assert(peer);*output=true;return true;}
static void combinedConstruction(size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Owner owners[4]={{0}};XL6_CodeOwner code[4];
    for(size_t i=0;i<4;i++)code[i]=(XL6_CodeOwner){sizeof(*code),owners+i,acquire,release};
    State state={0};Material source=material(3,&state);XL6_Closure* provider=target(code+1,&state);
    void* revocation=xl6TlsRevocationPolicyCreateV1(context(code),provider,false,0,31,64,64);assert(revocation);
    Environment* env=xrtMalloc(sizeof(*env));assert(env);env->state=&state;
    XL6_Closure* predicate=xl6ClosureRuntimeV2Get()->Create(code+3,(void(*)(void))additional,NULL,env,&layout,NULL,NULL,0);
    assert(predicate);
    static const XL6_ClassIdentitySchemaV3 peerSchema={sizeof(peerSchema),UINT64_C(0x63726c5065657243),1803,
        XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}};
    XL6_NativeClassResultContextV1 peer={{"payload",7,xl6TlsVerifiedPeerHandlePolicyV1Get()},code+2,&peerSchema};
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));assert(xrtMemDebugFailAfter(point));
    void* verifier=xl6TlsVerifierCreateCallbackRevocationV1(peer,source.trust,&source.policy,predicate,revocation);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result result=xl6ErrorScopeEnd(&errors);
    assert(hit?(!verifier&&!result.ok):(verifier&&result.ok));
    xrtErrorFree(result.error);xl6TlsVerifierDestroyV1(verifier);
    xl6TlsRevocationPolicyDestroyV1(revocation);xl6ClosureRuntimeV1Get()->Release(provider);
    xl6ClosureRuntimeV1Get()->Release(predicate);xl6X509StoreSnapshotDestroyV1(source.trust);dropState(&state);
    for(size_t i=0;i<4;i++)assert(!owners[i].pins&&owners[i].acquired==owners[i].released);
    assert(!xrtGetError());balanced(before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    for(size_t vector=0;vector<4;vector++)operation(vector,0,true,SIZE_MAX);
    for(unsigned mode=1;mode<4;mode++)operation(3,mode,true,SIZE_MAX);
    /* Explicit leniency is tested with a provider's ordinary unknown result. */
    operation(3,4,true,SIZE_MAX);
    for(unsigned invoking=0;invoking<2;invoking++)for(size_t point=0;point<128;point++)operation(3,0,invoking!=0,point);
    for(size_t point=0;point<128;point++)combinedConstruction(point);
    puts("TLS revocation: 4 signature families, selected-path source binding, strict unknown, checked callback failures, owning requests, independent Close leases, combined Context rollback, 384 OOM prefixes; memory balanced");return 0;
}
