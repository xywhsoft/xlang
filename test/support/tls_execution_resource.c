/* Fixed ordinary certificate chains, real managed TLS, literal loopback only.
 * No public endpoints, DNS, malformed traffic or security-style testing. */
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_execution_tls_internal.h"
#include "src/runtime/xl6_tls_role_context_internal.h"
#include "src/runtime/xl6_x509_store_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include "tls_role_context_vectors.h"
#include <stdio.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s error=%s\n",(unsigned)__LINE__,#C,xrtErrorMessage(xrtGetError()));abort();}}while(0)
typedef XL6_ExecutionResourceV1 Resource;
enum {CAPACITY=160};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prepare;
    const xrtownershipfinishv1* finish;bool done;} Node;
typedef struct Scene {XL6_ExecutionSourceV1* source;Resource *engine,*listener,*client,*peer;xfuture* pending;
    Node nodes[CAPACITY];size_t count;bool pinned,busy;} Scene;
static XL6_OwnershipCatalog* catalog;
static unsigned graphs,ordinary,scoped,budgets,failures,deferred,graceful;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount&&
        before.OverflowCount==after.OverflowCount&&before.UnderflowCount==after.UnderflowCount);
}
static void drain(XL6_ExecutionSourceV1** source)
{
    CHECK(xl6ExecutionSourceCloseV1(*source));xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    while(*source){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(source);CHECK(result.ok);if(result.ready)break;CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
}
static void contexts(void** client,void** server)
{
    xx509store* raw=xrtX509StoreCreate();CHECK(raw&&xrtX509StoreAdd(raw,pathVectors[0].root.Data,pathVectors[0].root.Size)==X509_VALUE);
    void* trust=xl6X509StoreVersionTakeV1(&raw,(int64_t)pathVectors[0].root.Size);CHECK(trust&&!raw);
    void* policy=xl6TlsVerificationPolicyCreateV1(64,16777216,64,100000,4096,67108864,false);CHECK(policy);
    void* fixed=xl6TlsVerificationPolicyWithTimeV1(policy,pathTime);CHECK(fixed);
    void* verifier=xl6TlsVerifierCreatePolicyV1(trust,fixed);CHECK(verifier);
    xbytesview chain[2];chain[0]=pathVectors[0].leaf;chain[1]=pathVectors[0].mid;
    xtlsidentity* identity=xrtTlsIdentityRsa(chain,2,tlsIdentityKeys[0]);CHECK(identity);
    *client=xl6TlsClientContextDefaultV1(verifier);*server=xl6TlsServerContextDefaultV1(identity);CHECK(*client&&*server);
    xrtTlsIdentityRelease(identity);xl6TlsVerifierDestroyV1(verifier);xl6TlsVerificationPolicyDestroyV1(fixed);
    xl6TlsVerificationPolicyDestroyV1(policy);xl6X509StoreSnapshotDestroyV1(trust);
}
static void create(Scene* s,xnetportbackend backend)
{
    void *client=NULL,*server=NULL;contexts(&client,&server);
    s->source=xl6ExecutionSourceCreateV1();CHECK(s->source);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=2;config.Backend=backend;
    s->engine=xl6ExecutionNetEngineCreateV1(s->source,&config);CHECK(s->engine&&xl6ExecutionNetEngineStartV1(s->engine));
    xtlslistenerconfig options;xrtTlsListenerConfigInit(&options);CHECK(xrtNetAddrLoopback(&options.Listen.Address,XNET_FAMILY_IPV4,0));
    XL6_ExecutionServicesV1 services={.source=s->source};
    s->listener=xl6TlsListenerCreateDefaultV1(services,s->engine,&options.Listen,server);CHECK(s->listener);
    XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsListenerEnterV1(s->listener,true,&scope));
    xnetaddr address;CHECK(xrtTlsListenerLocal(scope.listener,&address));xfuture* accepted=xrtTlsListenerAcceptAsync(scope.listener);CHECK(accepted);
    xl6ExecutionTlsLeaveV1(&scope);
    s->client=xl6TlsStreamCreateDefaultV1(services,s->engine,&address,client,(xstrview){"localhost",9},-1);CHECK(s->client);
    /* Source role wrappers disappear before the actual handshake: only XRT's
     * independently retained/copied immutable state may remain. */
    xl6TlsClientContextDestroyV1(client);xl6TlsServerContextDestroyV1(server);
    CHECK(xrtFutureWaitFor(accepted,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(accepted)==XFUTURE_RESOLVED);
    s->peer=xl6ExecutionTlsAdoptV1(s->source,s->engine,xrtFutureValue(accepted));CHECK(s->peer);xrtFutureDestroy(accepted);
    CHECK(xl6ExecutionTlsStreamEnterV1(s->client,false,&scope)&&xrtTlsStreamState(scope.stream)==XTLS_STREAM_OPEN);xl6ExecutionTlsLeaveV1(&scope);
    CHECK(xl6TlsStreamStateV1(s->client)==XTLS_STREAM_OPEN&&xl6TlsListenerStateV1(s->listener)==XTLS_LISTENER_OPEN);
    void* local=xl6TlsStreamLocalV1(s->client);void* remote=xl6TlsStreamRemoteV1(s->client);void* bound=xl6TlsListenerLocalV1(s->listener);
    CHECK(local&&remote&&bound&&xl6NetAddressPortV1(remote)==xl6NetAddressPortV1(bound));
    xl6NetAddressDestroyV1(local);xl6NetAddressDestroyV1(remote);xl6NetAddressDestroyV1(bound);
    XL6_ExecutionResourceSnapshotV1 state={.size=sizeof(state)};
    CHECK(xl6ExecutionResourceSnapshotV1Get(s->engine,&state)&&state.owners==4);xl6ExecutionResourceReleaseV1(s->engine);
    CHECK(xl6ExecutionResourceSnapshotV1Get(s->engine,&state)&&state.owners==3&&!state.closing);
}
static void roundtrip(Scene* s)
{
    XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsStreamEnterV1(s->peer,true,&scope));
    xfuture* read=xrtTlsStreamRecvAsync(scope.stream,7);CHECK(read);xl6ExecutionTlsLeaveV1(&scope);
    CHECK(xl6ExecutionTlsStreamEnterV1(s->client,true,&scope));xfuture* sent=xrtTlsStreamSendAsync(scope.stream,"abc\0def",7);CHECK(sent);xl6ExecutionTlsLeaveV1(&scope);
    CHECK(xrtFutureWaitFor(sent,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(sent)==XFUTURE_RESOLVED);
    CHECK(xrtFutureWaitFor(read,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(read)==XFUTURE_RESOLVED);
    xbytesview view=xrtNetBytesView(xrtFutureValue(read));CHECK(view.Size==7&&!memcmp(view.Data,"abc\0def",7));xrtFutureDestroy(read);xrtFutureDestroy(sent);
    CHECK(!xl6ExecutionTlsStreamEnterV1(s->listener,false,&scope)&&xrtErrorKind(xrtGetError())==XERR_TYPE);xrtClearError();
    CHECK(!scope.execution.resource&&!scope.stream&&!scope.listener);
}
static bool admit(xrtownershipref ref,void* raw)
{
    Scene* s=raw;const xrtownershipadapterv1* a=NULL;const xrtownershippreparationv1* p=NULL;const xrtownershipfinishv1* f=NULL;
    XL6_OwnershipAdmissionV1 result=xl6OwnershipCatalogProbeV1(ref,catalog,&a,&p,&f);
    if(result==XL6_OWNERSHIP_ADMISSION_BUSY)s->busy=true;
    return result==XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
static xrtownershipsnapshot* snapshot(Scene* s,xrtownershipscope* freeze)
{
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    for(;;){if(xrtOwnershipFreezeTryBegin(freeze)){
        s->busy=false;xrtownershipref anchors[CAPACITY+6]={xl6ExecutionSourceOwnershipV1(s->source),xl6ExecutionResourceOwnershipV1(s->engine),
            xl6ExecutionResourceOwnershipV1(s->listener),xl6ExecutionResourceOwnershipV1(s->client),xl6ExecutionResourceOwnershipV1(s->peer)};
        xrtownershipref slots[CAPACITY+5]={anchors[0],anchors[2],anchors[3],anchors[4]};size_t a=5,n=4;
        if(s->pending){anchors[a++]=xrtFutureOwnership(s->pending);slots[n++]=anchors[a-1];}
        if(s->pinned)for(size_t i=0;i<s->count;i++){anchors[a++]=s->nodes[i].ref;slots[n++]=s->nodes[i].ref;}
        xrtownershipsnapshot* proof=NULL;if(xrtOwnershipSnapshotCreate(anchors,a,slots,n,admit,s,&proof))return proof;
        CHECK(s->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(freeze));}
        CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
}
static void discover(Scene* s,xrtownershipsnapshot* proof)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);i++){
        xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);size_t at=0;
        while(at<s->count&&(s->nodes[at].ref.Data!=item.Reference.Data||s->nodes[at].ref.Ops!=item.Reference.Ops))at++;
        if(at<s->count)continue;
        CHECK(s->count<CAPACITY);Node* n=&s->nodes[s->count++];n->ref=item.Reference;
        CHECK(xl6OwnershipCatalogResolveV1(n->ref,catalog,&n->adapter,&n->prepare,&n->finish));
        CHECK(n->adapter->Hold(n->ref.Data)&&n->adapter->Claim(n->ref.Data,s));
    }s->pinned=true;
}
static void collect(Scene* s,unsigned order)
{
    xrtownershipscope freeze={0};xrtownershipsnapshot* proof=snapshot(s,&freeze);discover(s,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&freeze));
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    for(;;){bool all=true;
        for(size_t j=0;j<s->count;j++){size_t i=order&1?s->count-1-j:j;Node* n=&s->nodes[i];
            if(n->prepare){xrtownershipprepareresult result=n->prepare->Prepare(n->ref.Data,s);
                CHECK(result!=XRT_OWNERSHIP_PREPARE_FAILED);if(result!=XRT_OWNERSHIP_PREPARE_READY)all=false;}}
        proof=snapshot(s,&freeze);size_t old=s->count;discover(s,proof);xrtOwnershipSnapshotDestroy(proof);
        if(old!=s->count)all=false;
        for(size_t i=0;i<s->count;i++)if(s->nodes[i].prepare&&!s->nodes[i].prepare->Ready(s->nodes[i].ref.Data))all=false;
        if(all){for(size_t i=0;i<s->count;i++)s->nodes[i].adapter->Clear(s->nodes[i].ref.Data,s);CHECK(xrtOwnershipScopeEnd(&freeze));break;}
        CHECK(xrtOwnershipScopeEnd(&freeze));CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
    for(;;){bool all=true;
        for(size_t j=0;j<s->count;j++){size_t i=order&2?s->count-1-j:j;Node* n=&s->nodes[i];if(n->done)continue;
            xrtownershipfinishresult result=n->finish?n->finish->Step(n->ref.Data,s):
                (!n->adapter->Finish||n->adapter->Finish(n->ref.Data,s)?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_FAILED);
            CHECK(result!=XRT_OWNERSHIP_FINISH_FAILED);n->done=result==XRT_OWNERSHIP_FINISH_READY;if(!n->done)all=false;}
        if(all)break;
        CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
    xrtFutureDestroy(s->pending);xl6ExecutionResourceReleaseV1(s->client);xl6ExecutionResourceReleaseV1(s->peer);xl6ExecutionResourceReleaseV1(s->listener);
    drain(&s->source);for(size_t i=0;i<s->count;i++)s->nodes[i].adapter->Drop(s->nodes[i].ref.Data);graphs++;
}
static void ordinaryDrop(Scene* s)
{
    xrtFutureDestroy(s->pending);xl6ExecutionResourceReleaseV1(s->client);xl6ExecutionResourceReleaseV1(s->peer);xl6ExecutionResourceReleaseV1(s->listener);drain(&s->source);ordinary++;
}
typedef struct TlsCloseGate {xevent *entered,*resume;} TlsCloseGate;
static void holdTlsWorker(xnetworker* worker,void* raw)
{
    (void)worker;TlsCloseGate* gate=raw;CHECK(xrtEventSet(gate->entered));
    CHECK(xrtEventWaitFor(gate->resume,UINT64_C(10000000))==XWAIT_OK);
}
static void closeInspection(xrtownershipscope* scope)
{
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
}
static void gracefulLastOwner(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);
    TlsCloseGate gate={xrtEventCreate(true,false),xrtEventCreate(true,false)};CHECK(gate.entered&&gate.resume);
    XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsStreamEnterV1(s.client,true,&scope));
    xtlsstream* observer=xrtTlsStreamRef(scope.stream);CHECK(observer);
    xnetstream* tcp=xrtTlsStreamTransport(scope.stream);CHECK(tcp);
    xnetworker* worker=xrtNetStreamWorker(tcp);CHECK(worker);
    uint32 index=xrtNetWorkerIndex(worker);xl6ExecutionTlsLeaveV1(&scope);
    XL6_ExecutionScopeV1 engineScope={0};CHECK(xl6ExecutionResourceEnterV1(s.engine,false,&engineScope));
    CHECK(xrtNetEnginePost(engineScope.engine,index,holdTlsWorker,&gate));xl6ExecutionResourceLeaveV1(&engineScope);
    CHECK(xrtEventWaitFor(gate.entered,UINT64_C(10000000))==XWAIT_OK);
    unsigned char payload[65536];for(size_t i=0;i<sizeof(payload);++i)payload[i]=(unsigned char)i;
    CHECK(xl6ExecutionTlsStreamEnterV1(s.client,true,&scope));
    xfuture* sent=xrtTlsStreamSendAsync(scope.stream,payload,sizeof(payload));CHECK(sent);
    xl6ExecutionTlsLeaveV1(&scope);Resource* sender=s.client;
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipfinishv1* finish=NULL;xrtownershipscope inspection={0};closeInspection(&inspection);
    CHECK(xl6OwnershipCatalogProbeV1(xl6ExecutionResourceOwnershipV1(sender),catalog,&adapter,&preparation,&finish)==
        XL6_OWNERSHIP_ADMISSION_ADMITTED&&adapter->Hold(sender));
    uint64_t cycle=sender->cycle;CHECK(xrtOwnershipScopeEnd(&inspection));
    CHECK(xl6ExecutionResourceCloseV1(sender)&&xl6ExecutionResourceCloseV1(sender));
    xl6ExecutionResourceReleaseV1(sender);s.client=NULL;
    /* A catalog inspection pin is not a language owner. The real retirement
     * worker must observe owner zero before the queued send can progress. */
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    for(;;){
        closeInspection(&inspection);bool attempted=sender->cycle!=cycle&&!sender->retiring;
        CHECK(sender->owners==0&&sender->requested&&sender->source);
        CHECK(xrtOwnershipScopeEnd(&inspection));if(attempted)break;
        CHECK(!xrtDeadlineExpired(due));xrtThreadYield();
    }
    CHECK(xrtNetStreamState(tcp)!=XNET_STREAM_CLOSED);CHECK(xrtEventSet(gate.resume));
    CHECK(xrtFutureWaitFor(sent,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(sent)==XFUTURE_RESOLVED);
    xrtFutureDestroy(sent);size_t received=0;
    while(received<sizeof(payload)){
        CHECK(xl6ExecutionTlsStreamEnterV1(s.peer,true,&scope));
        xfuture* read=xrtTlsStreamRecvAsync(scope.stream,8192);CHECK(read);xl6ExecutionTlsLeaveV1(&scope);
        CHECK(xrtFutureWaitFor(read,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(read)==XFUTURE_RESOLVED);
        xbytesview view=xrtNetBytesView(xrtFutureValue(read));
        CHECK(view.Size&&view.Size<=sizeof(payload)-received&&!memcmp(view.Data,payload+received,view.Size));
        received+=view.Size;xrtFutureDestroy(read);
    }
    due=xrtDeadlineAfter(UINT64_C(10000000));
    while(xrtTlsStreamState(observer)!=XTLS_STREAM_CLOSED||xrtNetStreamState(tcp)!=XNET_STREAM_CLOSED){
        CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
    xrtTlsStreamDestroy(observer);adapter->Drop(sender);
    /* Join physical engine callback tails before destroying their event data. */
    xl6ExecutionResourceReleaseV1(s.peer);xl6ExecutionResourceReleaseV1(s.listener);drain(&s.source);
    xrtEventDestroy(gate.entered);xrtEventDestroy(gate.resume);balanced(before);++graceful;

    xrtMemDebugSnapshot(&before);s=(Scene){0};create(&s,backend);
    CHECK(xl6ExecutionResourceCloseV1(s.client)&&xl6ExecutionResourceCancelV1(s.client));
    CHECK(xl6ExecutionResourceCloseV1(s.client));
    CHECK(xrtAtomic32Load(&((XL6_TlsStreamResourceV1*)s.client)->closeMode,XMEMORY_ACQUIRE)==XL6_TLS_CLOSE_ABORT_V1);
    xl6ExecutionResourceReleaseV1(s.client);xl6ExecutionResourceReleaseV1(s.peer);
    xl6ExecutionResourceReleaseV1(s.listener);drain(&s.source);balanced(before);
}
static void construction(xnetportbackend backend)
{
    unsigned success=0;
    for(unsigned budget=0;budget<96&&success<2;budget++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void *client=NULL,*server=NULL;contexts(&client,&server);
        Scene s={0};s.source=xl6ExecutionSourceCreateV1();CHECK(s.source);
        xnetengineconfig engine;xrtNetEngineConfigInit(&engine);engine.Backend=backend;engine.Workers=2;
        s.engine=xl6ExecutionNetEngineCreateV1(s.source,&engine);CHECK(s.engine&&xl6ExecutionNetEngineStartV1(s.engine));
        xtlslistenerconfig config;xrtTlsListenerConfigInit(&config);CHECK(xrtNetAddrLoopback(&config.Listen.Address,XNET_FAMILY_IPV4,0));
        s.listener=xl6ExecutionTlsListenV1(s.source,s.engine,server,&config);CHECK(s.listener);xl6TlsServerContextDestroyV1(server);
        XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsListenerEnterV1(s.listener,true,&scope));
        xnetaddr address;CHECK(xrtTlsListenerLocal(scope.listener,&address));xl6ExecutionTlsLeaveV1(&scope);
        CHECK(xrtMemDebugFailAfter(budget));
        Resource* result=xl6ExecutionTlsConnectV1(s.source,s.engine,&address,0,client,(xstrview){"localhost",9},(xstrview){0},NULL,NULL);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();budgets++;
        if(!result){CHECK(hit&&xrtGetError());xrtClearError();failures++;success=0;}else{CHECK(!xrtGetError());success++;}
        /* A hit in an accepted asynchronous worker may fail the handshake,
         * not the synchronous constructor. In either case retire the REAL
         * native operation; no success/NULL inference from the global budget. */
        xl6TlsClientContextDestroyV1(client);xl6ExecutionResourceReleaseV1(result);
        xl6ExecutionResourceReleaseV1(s.listener);xl6ExecutionResourceReleaseV1(s.engine);drain(&s.source);balanced(before);
    }CHECK(success==2&&failures);
}
static struct {XL6_ExecutionSourceV1* source;xnetaddr address;xtlsclientconfig client;} closingFactory;
static xtlsstream* closeBeforePublication(xnetengine* engine,Resource* data,
    const xtlsstreamownershipv1* policy,const XL6_TlsStreamCreateOptionsV1* input)
{
    CHECK(input->Build==closeBeforePublication);
    xtlsstream* native=xrtTlsStreamConnectOwnedV1(engine,&closingFactory.address,0,NULL,&closingFactory.client,NULL,data,policy);
    CHECK(native&&xl6ExecutionSourceCloseV1(closingFactory.source));return native;
}
static void publicationRace(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void *client=NULL,*server=NULL;contexts(&client,&server);Scene s={0};
    s.source=xl6ExecutionSourceCreateV1();CHECK(s.source);xnetengineconfig engine;xrtNetEngineConfigInit(&engine);engine.Backend=backend;
    s.engine=xl6ExecutionNetEngineCreateV1(s.source,&engine);CHECK(s.engine&&xl6ExecutionNetEngineStartV1(s.engine));
    xtlslistenerconfig config;xrtTlsListenerConfigInit(&config);CHECK(xrtNetAddrLoopback(&config.Listen.Address,XNET_FAMILY_IPV4,0));
    s.listener=xl6ExecutionTlsListenV1(s.source,s.engine,server,&config);CHECK(s.listener);
    XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsListenerEnterV1(s.listener,true,&scope));
    CHECK(xrtTlsListenerLocal(scope.listener,&closingFactory.address));xl6ExecutionTlsLeaveV1(&scope);
    closingFactory.source=s.source;xrtTlsClientConfigInit(&closingFactory.client);
    const XL6_TlsClientContextV1* context=client;closingFactory.client.Context=context->parameters;
    closingFactory.client.Verifier=context->verifier.value;closingFactory.client.ServerName=(xstrview){"localhost",9};
    const XL6_TlsStreamCreateOptionsV1 tls={.engine=s.engine,.Build=closeBeforePublication};
    const XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TLS_STREAM_V1,.deferRollback=true,.value.tlsStream=&tls};
    CHECK(!xl6ExecutionCreateInternalV1(s.source,xl6ExecutionTlsStreamFamilyV1Get(),&options)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    xl6TlsClientContextDestroyV1(client);xl6TlsServerContextDestroyV1(server);
    xl6ExecutionResourceReleaseV1(s.listener);xl6ExecutionResourceReleaseV1(s.engine);drain(&s.source);
    memset(&closingFactory,0,sizeof(closingFactory));balanced(before);deferred++;
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));const XL6_OwnershipDomainV1* domains[]={
        #include "tls-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
#ifdef _WIN32
        backend=XNET_PORT_IOCP;
#else
        backend=XNET_PORT_EPOLL;
#endif
    }
    for(unsigned order=0;order<6;order++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene s={0};create(&s,backend);roundtrip(&s);
        if(order<4){XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsStreamEnterV1(s.peer,true,&scope));
            s.pending=xrtTlsStreamRecvAsync(scope.stream,1);CHECK(s.pending&&xrtFutureState(s.pending)==XFUTURE_PENDING);xl6ExecutionTlsLeaveV1(&scope);collect(&s,order);
        }else{
            if(order==5){XL6_TlsScopeV1 scope={0};CHECK(xl6ExecutionTlsStreamEnterV1(s.client,false,&scope));
                Resource* client=s.client;void* owned=client;
                xvalue* handle=xl6NativeHandleTakeV1(&owned,xl6TlsStreamHandlePolicyV1Get());CHECK(handle&&!owned);
                s.client=NULL;xrtValueRelease(handle);
                CHECK(!xl6ExecutionResourceNativePinV1(client)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
                CHECK(xrtTlsStreamState(scope.stream)==XTLS_STREAM_OPEN);xl6ExecutionTlsLeaveV1(&scope);scoped++;}
            ordinaryDrop(&s);
        }balanced(before);
    }
    gracefulLastOwner(backend);construction(backend);publicationRace(backend);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("managed TLS resources: graphs=%u ordinary=%u scoped-last-owner=%u deferred=%u budgets=%u failures=%u; graceful-last-owner=%u abort-wins=1; real Data credits, parent owners, exact kinds, NUL roundtrip; balanced backend=%d\n",graphs,ordinary,scoped,deferred,budgets,failures,graceful,(int)backend);return 0;
}
