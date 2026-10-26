/* Real resident factories + actual xvalue/Verifier/Context/Session graph.
 * Normal fixed DER only; no network, handshake, malformed crypto or security tests. */
#include "src/runtime/xl6_tls_verifier_context.h"
#include "src/runtime/xl6_tls_role_context_internal.h"
#include "src/runtime/xl6_tls_session_internal.h"
#include "src/runtime/xl6_x509_store_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "tls_verifier_vectors.h"
static unsigned Calls,Drops;
static int DropMode;
static void* captureRef(void* raw){return xrtValueRetain(raw);}
static void captureDrop(void* raw)
{
    Drops++;xrtValueRelease(raw);
    if(DropMode==1)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if(DropMode==2)xrtSetErrorKind(XERR_IO);
}
static xrtownershipref captureOwnership(const void* raw){return xrtValueOwnership(raw);}
static bool additional(void* raw,const xtlsverifiedpeer* verified)
{
    assert(xrtValueType(raw)==XVALUE_OBJECT&&verified&&verified->Peer&&verified->Anchor&&verified->PathCount==2);
    assert(verified->Peer->Role==XTLS_SERVER&&verified->Peer->Name.Size==9&&
        !memcmp(verified->Peer->Name.Data,"localhost",9));Calls++;return true;
}
static const XL6_TlsPeerPolicyOpsV1 Capture={captureRef,captureDrop,captureOwnership,additional};
static XL6_TlsVerificationPolicyV1 policy(void)
{
    XL6_TlsVerificationPolicyV1 p;assert(xl6TlsVerificationPolicyInitV1(&p));p.fixedTime=true;p.at=pathTime;return p;
}
static void* trust(void)
{
    xx509store* store=xrtX509StoreCreate();
    assert(store&&xrtX509StoreAdd(store,path_rsa_root,sizeof(path_rsa_root))==X509_VALUE);
    void* result=xl6X509StoreVersionTakeV1(&store,sizeof(path_rsa_root));assert(result&&!store);return result;
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
typedef struct Child { xrtownershipref value;unsigned count; } Child;
static bool child(xrtownershipref ref,void* raw)
{Child* c=raw;c->value=ref;c->count++;return true;}
static void chain(XL6_TlsVerifierV1* facade)
{
    xx509cert certificates[2];assert(xrtX509Parse(path_rsa_leaf,sizeof(path_rsa_leaf),&certificates[0])&&
        xrtX509Parse(path_rsa_mid,sizeof(path_rsa_mid),&certificates[1]));
    assert(xrtTlsVerifierVerify(facade->value,XTLS_SERVER,(xstrview){"localhost",9},certificates,2)&&!xrtGetError());
}
static void cycle(unsigned kind,XL6_OwnershipCatalog* catalog)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned priorDrops=Drops,priorCalls=Calls;
    XL6_TlsVerificationPolicyV1 p=policy();void* source=trust();xvalue* capture=xrtValueObject();assert(capture);
    XL6_TlsVerifierV1* facade=xl6TlsVerifierCreatePeerPolicyInternalV1(source,&p,(XL6_TlsPeerPolicyV1){capture,&Capture});assert(facade);
    xl6X509StoreSnapshotDestroyV1(source);chain(facade);assert(Calls==priorCalls+1);
    xrtownershipscope freeze={0};Child context={0},ownedCapture={0};size_t refs=0;
    assert(xrtOwnershipFreezeTryBegin(&freeze));xrtownershipref native=xrtTlsVerifierOwnership(facade->value);
    assert(native.Ops->Trace(native.Data,child,&context)&&context.count==1);
    assert(context.value.Ops==xl6TlsVerifierContextNativePolicyV1Get()->Ops&&
        context.value.Ops->Trace(context.value.Data,child,&ownedCapture)&&ownedCapture.count==1&&
        ownedCapture.value.Data==capture&&ownedCapture.value.Ops==xrtValueOwnership(capture).Ops);
    assert(xrtOwnershipScopeEnd(&freeze));
    void* payload=facade;const XL6_NativeHandlePolicyV1* handlePolicy=xl6TlsVerifierHandlePolicyV1Get();
    if(kind){
        void* client=xl6TlsClientContextDefaultV1(facade);assert(client);
        if(kind==1){payload=client;handlePolicy=xl6TlsClientContextHandlePolicyV1Get();}
        else{
            payload=xl6TlsSessionClientCreateV1(client,(xstrview){"localhost",9},-1,(xstrview){0},-1);assert(payload);
            handlePolicy=xl6TlsSessionHandlePolicyV1Get();xl6TlsClientContextDestroyV1(client);
        }
        xl6TlsVerifierDestroyV1(facade);
    }
    assert(xrtOwnershipFreezeTryBegin(&freeze)&&native.Ops->Count(native.Data,&refs)&&refs==1);
    assert(xrtOwnershipScopeEnd(&freeze));
    xvalue* handle=xl6NativeHandleTakeV1(&payload,handlePolicy);assert(handle&&!payload);
    assert(xrtValueObjectSetNewPostCommit(capture,(xstrview){"consumer",8},handle));
    xrtownershipref root=xrtValueOwnership(capture);xrtValueRelease(capture); /* actual callback ref is now the sole bag owner */
    assert(xrtOwnershipFreezeTryBegin(&freeze));xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.NodeCount>=4&&graph.EdgeCount>=4&&
        graph.ExternalRootCount==0&&graph.ReachableAnchorCount==0);
    assert(xrtOwnershipScopeEnd(&freeze));
    XL6_OwnershipPlan* plan=NULL;XL6_OwnershipPlanResult state=
        xl6OwnershipPlanTryCreateV6(root,xl6OwnershipCatalogProbeV1,NULL,false,catalog,&plan);
    assert(plan&&(state==XL6_OWNERSHIP_PLAN_READY||state==XL6_OWNERSHIP_PLAN_PREPARATIONS||state==XL6_OWNERSHIP_PLAN_FINALIZERS));
    bool committed=false;
    for(unsigned pass=0;pass<32&&!committed;pass++){
        state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_PREPARATIONS)assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
        else if(state==XL6_OWNERSHIP_PLAN_FINALIZERS)assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
        else{assert(state==XL6_OWNERSHIP_PLAN_READY);state=xl6OwnershipPlanTryCommit(plan);committed=state==XL6_OWNERSHIP_PLAN_READY;
            assert(committed||state==XL6_OWNERSHIP_PLAN_PREPARATIONS||state==XL6_OWNERSHIP_PLAN_FINALIZERS);}
    }
    assert(committed&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY&&Drops==priorDrops+1&&!xrtGetError());balanced(before);
}
static void cleanupOutcomes(void)
{
    for(int mode=0;mode<3;mode++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned drops=Drops;
        xvalue* capture=xrtValueObject();assert(capture);XL6_TlsVerificationPolicyV1 p=policy();
        XL6_TlsVerifierContextV1* context=xl6TlsVerifierContextCreateV1(&p,(XL6_TlsPeerPolicyV1){capture,&Capture});assert(context);
        xrtownershipref ref={context,xl6TlsVerifierContextNativePolicyV1Get()->Ops};xrtownershipscope freeze={0};
        assert(xrtOwnershipFreezeTryBegin(&freeze));const xrtownershipadapterv1* adapter=xl6TlsVerifierContextOwnershipAdapterV1(ref);
        assert(adapter&&adapter->Hold(context)&&adapter->Claim(context,&drops));adapter->Clear(context,&drops);
        assert(xrtOwnershipScopeEnd(&freeze));DropMode=mode;
        if(!mode)xrtSetErrorKind(XERR_RANGE); /* caller failure must not poison this duty */
        for(unsigned retry=0;retry<2;retry++){
            assert(adapter->Finish(context,&drops)==(mode==0)&&Drops==drops+1);
            if(!mode&&retry==0)assert(xrtErrorKind(xrtGetError())==XERR_RANGE);
            if(mode==1)assert(!xrtGetError());
            if(mode==2)assert(xrtErrorKind(xrtGetError())==XERR_IO);
            xrtClearError();
        }
        adapter->Drop(context);xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
        xl6TlsVerifierContextDropV1(context);xl6_error_result result=xl6ErrorScopeEnd(&errors);
        assert(result.ok==(mode==0));if(mode==1)assert(!result.error);if(mode==2)assert(xrtErrorKind(result.error)==XERR_IO);
        xrtErrorFree(result.error);xrtValueRelease(capture);DropMode=0;xrtClearError();balanced(before);
    }
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainTlsVerifierV1Get(),xl6OwnershipDomainTlsClientContextV1Get(),
        xl6OwnershipDomainTlsSessionV1Get(),xl6OwnershipDomainNetBufferNativeV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,4);assert(catalog);
    for(unsigned kind=0;kind<3;kind++)cycle(kind,catalog);
    cleanupOutcomes();xl6OwnershipCatalogDestroyV1(catalog);balanced(before);
    printf("TLS verifier consumers: three actual consumer cycles collected, normal trust before policy, exact native/context/capture edges, own cleanup outcome and nil-diagnostic retry; memory balanced\n");
    return 0;
}
