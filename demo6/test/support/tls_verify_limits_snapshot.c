/* Offline normal certificates, exact spans, ownership and resource contracts.
 * No transport, malformed crypto, or native security suites. */
#if defined(XL6_TEST_XRUNTIME_DECL)
#include "include/xruntime_decl.h"
#else
#include "include/xrt_decl.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "x509_path_vectors.h"

static unsigned Checks, Failures;
typedef struct VerifyState { xtime time; unsigned calls, releases; } VerifyState;
static xtime clockValue(void* p) { return ((VerifyState*)p)->time; }
static void releaseState(void* p) { ((VerifyState*)p)->releases++; }
static xtlsverifydecision acceptPeer(const xtlspeer* peer,void* p)
{ assert(peer->CertificateCount==2);((VerifyState*)p)->calls++;return XTLS_VERIFY_ACCEPT; }
static bool acceptPath(const xtlsverifiedpeer* peer,void* p)
{ assert(peer->PathCount==2 && peer->Anchor && peer->Peer->CertificateCount==2);((VerifyState*)p)->calls++;return true; }
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void check(const xtlspeer* peer,const xx509store* store,const xtlsverifylimits* limits,bool expected,xerrkind kind)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);VerifyState state={0};
    assert(xrtTlsPeerVerifyLimited(peer,store,limits,acceptPath,&state)==expected && state.calls==(expected?1u:0u));
    if(!expected)assert(xrtErrorKind(xrtGetError())==kind);else assert(!xrtGetError());
    xrtClearError();balanced(before);Checks++;
}
static void exercise(const PathVector* vector)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xx509cert chain[2];xx509store* store=xrtX509StoreCreate();
    assert(store && xrtX509Parse(vector->leaf.Data,vector->leaf.Size,&chain[0]) &&
        xrtX509Parse(vector->mid.Data,vector->mid.Size,&chain[1]) &&
        xrtX509StoreAdd(store,vector->root.Data,vector->root.Size)==X509_VALUE);
    /* The embedded product must retain its microsecond contract. */
    assert(chain[0].NotBefore==1577836800000000LL);
    xtlspeer peer={XTLS_SERVER,{"localhost",9},pathTime,chain,2};
    xtlsverifylimits defaults;xrtTlsVerifyLimitsInit(&defaults);
    assert(defaults.MaxPeerCertificates==64 && defaults.MaxPeerBytes==16777216 &&
        defaults.MaxPathCertificates==64 && defaults.MaxSearchSteps==100000 &&
        defaults.MaxTrustCertificates==4096 && defaults.MaxTrustBytes==67108864);
    xtlsverifylimits exact={2,vector->leaf.Size+vector->mid.Size,2,5,1,vector->root.Size};
    check(&peer,store,&defaults,true,0);check(&peer,store,&exact,true,0);
    xtlsverifylimits limits=exact;limits.MaxPeerCertificates--;check(&peer,store,&limits,false,XERR_RANGE);
    limits=exact;limits.MaxPeerBytes--;check(&peer,store,&limits,false,XERR_RANGE);
    limits=exact;limits.MaxPathCertificates--;check(&peer,store,&limits,false,XERR_RANGE);
    limits=exact;limits.MaxSearchSteps--;check(&peer,store,&limits,false,XERR_RANGE);
    limits=exact;limits.MaxTrustBytes--;check(&peer,store,&limits,false,XERR_RANGE);
    memset(&limits,0,sizeof(limits));check(&peer,store,&limits,true,0);
    assert(xrtTlsPeerVerify(&peer,store,NULL,NULL));
    xtlspeer early=peer;early.Time=chain[0].NotBefore-1;
    (void)xrtTlsVerifierCreateLimited(NULL,NULL);
    assert(!xrtTlsPeerVerifyLimited(&early,store,&defaults,NULL,NULL) &&
        xrtErrorKind(xrtGetError())==XERR_PROTOCOL && !xrtErrorCause(xrtGetError()));
    xrtClearError();Checks++;
    VerifyState state={pathTime,0,0};xtlsverifierconfig config;xrtTlsVerifierConfigInit(&config);
    config.Store=store;config.Time=clockValue;config.Context=&state;config.Release=releaseState;
    limits=exact;limits.MaxTrustBytes--;
    assert(!xrtTlsVerifierCreateLimited(&config,&limits) && !state.releases && xrtErrorKind(xrtGetError())==XERR_RANGE);
    xrtClearError();Checks++;
    limits=exact;xtlsverifier* verifier=xrtTlsVerifierCreateLimited(&config,&limits);
    assert(verifier && xrtTlsVerifierRetain(verifier)==verifier);limits.MaxPeerBytes=1;
    xrtX509StoreFree(store);store=NULL;
    assert(xrtTlsVerifierVerify(verifier,peer.Role,peer.Name,chain,2));
    xrtTlsVerifierRelease(verifier);assert(!state.releases);
    xrtTlsVerifierRelease(verifier);assert(state.releases==1);
    state.calls=state.releases=0;config.Store=NULL;config.Verify=acceptPeer;limits=exact;limits.MaxPeerBytes--;
    verifier=xrtTlsVerifierCreateLimited(&config,&limits);
    assert(verifier && !xrtTlsVerifierVerify(verifier,peer.Role,peer.Name,chain,2) &&
        !state.calls && xrtErrorKind(xrtGetError())==XERR_RANGE);
    xrtClearError();xrtTlsVerifierRelease(verifier);assert(state.releases==1);state.releases=0;
    store=xrtX509StoreCreate();assert(store &&
        xrtX509StoreAdd(store,vector->root.Data,vector->root.Size)==X509_VALUE &&
        xrtX509StoreAdd(store,vector->mid.Data,vector->mid.Size)==X509_VALUE);
    config.Store=store;config.Verify=NULL;
    assert(!xrtTlsVerifierCreateLimited(&config,&exact) && !state.releases && xrtErrorKind(xrtGetError())==XERR_RANGE);
    xrtClearError();Checks++;
    bool complete=false;
    for(size_t budget=0;budget<256;budget++){
        xmemdebugsnapshot allocationBefore;xrtMemDebugSnapshot(&allocationBefore);
        assert(xrtMemDebugFailAfter(budget));verifier=xrtTlsVerifierCreateLimited(&config,&defaults);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){assert(!verifier && !state.releases);Failures++;}
        else {assert(verifier);complete=true;}
        xrtTlsVerifierRelease(verifier);xrtClearError();balanced(allocationBefore);if(complete)break;
    }
    assert(complete && state.releases==1);state.releases=0;config.Store=NULL;config.Verify=acceptPeer;
    assert(!xrtTlsVerifierCreateLimited(&config,NULL) && !state.releases && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    xrtClearError();check(&peer,store,NULL,false,XERR_ARGUMENT);
    xrtX509StoreFree(store);xrtClearError();balanced(before);
}
int main(void)
{
    for(size_t i=0;i<sizeof(pathVectors)/sizeof(*pathVectors);i++)exercise(&pathVectors[i]);
    assert(Checks==48 && Failures && xrtMemDebugReset());
    printf("TLS verifier budgets: 4 normal families; %u exact boundaries; %u allocation failures; owned snapshots balanced\n",Checks,Failures);
    return 0;
}
