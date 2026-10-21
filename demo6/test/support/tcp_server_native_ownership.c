/* Independent XRT c09e357a comparison: only include/time ABI adaptation.
 * Ordinary self-created literal loopback; no external or security traffic. */
#ifdef SERVER_PINNED_XRUNTIME
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#else
#define XRT_IMPLEMENTATION
#include "xrt.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool condition,const char* message)
{if(!condition){fprintf(stderr,"[FAIL] %s\n",message);exit(1);}}
#define testMemoryDebugDrain(Message) testRequire(xrtMemDebugReset(),(Message))
static bool testTcpServerEngineDestroy(xnetengine* engine)
{
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    for(;;){xnetretireresult result=xrtNetEngineTryDestroy(engine);
        if(result==XNET_RETIRE_READY)return true;
        if(result!=XNET_RETIRE_BUSY||xrtDeadlineExpired(due))return false;
        xrtThreadYield();}
}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %u: ",(unsigned)__LINE__);testRequire(false,#c);}}while(0)
enum {MAX_NODES=64};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prep;bool ready;} Node;
typedef struct Scene {
    xnetengine* engine;xnetserver* server;xnetstream* client;xfuture* future;
    Node nodes[MAX_NODES];Node* races[2];size_t count;bool pinned,busy;
} Scene;
static unsigned graphs,refusals,oomBudgets,oomFailures,transfers,dropRaces;
static void freeze(xrtownershipscope* scope)
{
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
}
static void idle(xnetengine* engine,xrtownershipscope* scope)
{
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    for(;;){freeze(scope);if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(engine))return;
        CHECK(xrtOwnershipScopeEnd(scope));CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
}
static void balanced(xmemdebugsnapshot before)
{
    xrtClearError();xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);
    CHECK(before.LiveCount==end.LiveCount&&before.LiveBytes==end.LiveBytes);
    CHECK(end.AllocCount-before.AllocCount==end.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==end.InvalidFreeCount&&before.DoubleFreeCount==end.DoubleFreeCount&&
        before.UseAfterFreeCount==end.UseAfterFreeCount&&before.OverflowCount==end.OverflowCount&&before.UnderflowCount==end.UnderflowCount);
}
static const xrtownershipadapterv1* resolve(Scene* scene,xrtownershipref ref,const xrtownershippreparationv1** prep)
{
    const xrtownershipquiescencev1* quiet[]={xrtNetEngineOwnershipQuiescenceV1Get(),xrtNetStreamOwnershipQuiescenceV1Get(),
        xrtNetListenerOwnershipQuiescenceV1Get(),xrtNetServerOwnershipQuiescenceV1Get(),xrtNetServerBindingOwnershipQuiescenceV1Get(),
        xrtNetServerFutureOwnershipQuiescenceV1Get(),xrtNetBufPoolOwnershipQuiescenceV1Get(),xrtNetTcpFutureOwnershipQuiescenceV1Get(),
        xrtFutureOwnershipQuiescenceV1Get(),xrtCancelOwnershipQuiescenceV1Get(),xrtCancelWatchOwnershipQuiescenceV1Get()};
    for(size_t i=0;i<sizeof(quiet)/sizeof(*quiet);++i)
        if(ref.Ops==quiet[i]->Ops&&quiet[i]->Busy(ref.Data)){scene->busy=true;return NULL;}
    const xnetruntimeownershipv1* runtime[]={xrtNetServerRuntimeOwnershipV1Get(),xrtNetListenerRuntimeOwnershipV1Get(),xrtNetStreamRuntimeOwnershipV1Get()};
    const xnetlistenereventsownershipv1* events[]={xrtNetServerListenerEventPolicyV1Get()};
    const xfutureproducerownershipv1* producers[]={xrtNetServerFutureProducerPolicyV1Get(),xrtNetTcpFutureProducerPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[]={xrtNetServerFutureCancelPolicyV1Get(),xrtNetTcpFutureCancelPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[]={xrtNetServerFutureStreamPayloadPolicyV1Get(),xrtNetTcpFutureBytesPayloadPolicyV1Get(),xrtNetTcpFutureStreamPayloadPolicyV1Get()};
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.PayloadPolicies=payloads;admission.PayloadPolicyCount=3;
    admission.ProducerPolicies=producers;admission.ProducerPolicyCount=2;
    admission.CancelWatchPolicies=cancels;admission.CancelWatchPolicyCount=2;
    const xrtownershipadapterv1* adapter=xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtime,3,prep);
    if(!adapter)adapter=xrtNetServerOwnershipAdapterV1(ref,prep);
    if(!adapter)adapter=xrtNetListenerOwnershipAdapterV2(ref,events,1,prep);
    if(!adapter)adapter=xrtNetStreamOwnershipAdapterV1(ref,prep);
    if(!adapter)adapter=xrtNetServerFutureOwnershipAdapterV1(ref,prep);
    if(!adapter)adapter=xrtNetTcpFutureOwnershipAdapterV1(ref,prep);
    if(!adapter)adapter=xrtNetBufPoolOwnershipAdapterV1(ref,prep);
    if(!adapter)adapter=xrtFutureOwnershipAdapterV4(ref,&admission,prep);
    if(!adapter)adapter=xrtCancelOwnershipAdapterV2(ref,cancels,2,prep);
    if(!adapter)adapter=xrtCancelWatchOwnershipAdapterV1(ref,cancels,2,prep);
    if(!adapter)adapter=xrtNetBytesOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtErrorOwnershipAdapterV1(ref);
    return adapter;
}
static bool admit(xrtownershipref ref,ptr data)
{const xrtownershippreparationv1* prep=NULL;return resolve(data,ref,&prep)!=NULL;}
static xrtownershipsnapshot* snapshot(Scene* scene,xrtownershipscope* scope)
{
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    for(;;){freeze(scope);scene->busy=false;xrtownershipref roots[MAX_NODES+8],slots[MAX_NODES+8];size_t count=0,owned=0;
        roots[count++]=xrtNetEngineOwnership(scene->engine);roots[count++]=xrtNetServerOwnership(scene->server);
        if(scene->client)roots[count++]=xrtNetStreamOwnership(scene->client);
        if(scene->future)roots[count++]=xrtFutureOwnership(scene->future);
        for(size_t i=0;i<count;++i)slots[owned++]=roots[i];
        if(scene->pinned)for(size_t i=0;i<scene->count;++i){slots[owned++]=scene->nodes[i].ref;roots[count++]=scene->nodes[i].ref;}
        for(size_t i=0;i<2;++i)if(scene->races[i]){slots[owned++]=scene->races[i]->ref;slots[owned++]=scene->races[i]->ref;}
        xrtownershipsnapshot* proof=NULL;
        if(xrtOwnershipSnapshotCreate(roots,count,slots,owned,admit,scene,&proof))return proof;
        if(!scene->busy)fprintf(stderr,"snapshot metadata refused at nodes=%zu, server refs=%d bindings=%u active=%u\n",scene->count,
            (int)__xrtAtomicRefLoad(&scene->server->References),(unsigned)xrtAtomic32Load(&scene->server->Bindings,XMEMORY_ACQUIRE),
            (unsigned)xrtAtomic32Load(&scene->server->OwnershipActive,XMEMORY_ACQUIRE));
        CHECK(scene->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(scope));CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
}
typedef struct DropRace {Node* node;xatomic32 entered,go;} DropRace;
static int32 raceDrop(ptr data)
{
    DropRace* race=data;xrtAtomic32FetchAdd(&race->entered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&race->go,XMEMORY_ACQUIRE))xrtThreadYield();
    race->node->adapter->Drop(race->node->ref.Data);return 0;
}
static void concurrentDrop(Node* node)
{
    DropRace race={0};race.node=node;xrtAtomic32Init(&race.entered,0);xrtAtomic32Init(&race.go,0);
    xthread* a=xrtThreadCreate(raceDrop,&race,0);xthread* b=xrtThreadCreate(raceDrop,&race,0);CHECK(a&&b);
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    while(xrtAtomic32Load(&race.entered,XMEMORY_ACQUIRE)!=2){CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
    xrtAtomic32Store(&race.go,1,XMEMORY_RELEASE);node->adapter->Drop(node->ref.Data);
    CHECK(xrtThreadWaitFor(a,UINT64_C(10000000))==XWAIT_OK&&xrtThreadExitCode(a)==0);
    CHECK(xrtThreadWaitFor(b,UINT64_C(10000000))==XWAIT_OK&&xrtThreadExitCode(b)==0);xrtThreadDestroy(a);xrtThreadDestroy(b);++dropRaces;
}
static void examine(Scene* scene,xrtownershipsnapshot* proof,bool pin)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode node;CHECK(xrtOwnershipSnapshotNode(proof,i,&node));
        if(node.Reachable)fprintf(stderr,"unreported native root at node %zu refs=%zu\n",i,node.StrongCount);
        CHECK(!node.Reachable);
        if(!pin)continue;
        size_t j=0;for(;j<scene->count;++j)if(scene->nodes[j].ref.Data==node.Reference.Data&&scene->nodes[j].ref.Ops==node.Reference.Ops)break;
        if(j<scene->count)continue;
        CHECK(scene->count<MAX_NODES);Node* saved=&scene->nodes[scene->count++];saved->ref=node.Reference;
        saved->adapter=resolve(scene,node.Reference,&saved->prep);CHECK(saved->adapter);
        CHECK(saved->adapter->Hold(saved->ref.Data)&&saved->adapter->Claim(saved->ref.Data,scene));
    }
}
static void create(Scene* scene,xnetportbackend backend)
{
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Backend=backend;ec.Workers=2;ec.NodeCacheBytes=0;
    scene->engine=xrtNetEngineCreate(&ec);CHECK(scene->engine&&xrtNetEngineStart(scene->engine));
    xnetserverconfig config;xrtNetServerConfigInit(&config);xnetlistenconfig second=config.Listen;
    CHECK(xrtNetAddrLoopback(&config.Listen.Address,XNET_FAMILY_IPV4,0));CHECK(xrtNetAddrLoopback(&second.Address,XNET_FAMILY_IPV4,0));
    config.Listen.Stream.ReadMode=XNET_STREAM_READ_PROBE;second.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    config.Additional=&second;config.AdditionalCount=1;config.AcceptQueueLimit=8;
    scene->server=xrtNetServerStart(scene->engine,&config,NULL,NULL,NULL);CHECK(scene->server);
    CHECK(xrtNetServerEndpointCount(scene->server)==2&&xrtNetServerListenerCount(scene->server)==2);
}
static void graphCase(xnetportbackend backend,bool pending)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Scene scene={0};create(&scene,backend);
    if(pending){scene.future=xrtNetServerAcceptAsync(scene.server);CHECK(scene.future);}
    xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&scene,&scope);
    if(!pending)CHECK(xrtOwnershipSnapshotNodeCount(proof)==8);
    examine(&scene,proof,false);
    const xrtownershippreparationv1* unchanged=(const xrtownershippreparationv1*)&scene;
    xnetruntimeownershipv1 copied=*xrtNetServerRuntimeOwnershipV1Get();
    const xnetruntimeownershipv1* wrong[]={&copied,xrtNetListenerRuntimeOwnershipV1Get(),xrtNetStreamRuntimeOwnershipV1Get()};
    CHECK(!xrtNetEngineOwnershipAdapterV3(xrtNetEngineOwnership(scene.engine),NULL,0,NULL,0,NULL,0,wrong,3,&unchanged));
    CHECK(unchanged==(const xrtownershippreparationv1*)&scene);++refusals;
    xnetlistenereventsownershipv1 events=*xrtNetServerListenerEventPolicyV1Get();const xnetlistenereventsownershipv1* forged[]={&events};
    xrtownershipref listener=xrtNetListenerOwnership(scene.server->Children[0].Listener);
    CHECK(!xrtNetListenerOwnershipAdapterV2(listener,forged,1,&unchanged)&&unchanged==(const xrtownershippreparationv1*)&scene);++refusals;
    examine(&scene,proof,true);scene.pinned=true;xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
    CHECK(!xrtNetServerAcceptAsync(scene.server));xrtClearError();
    CHECK(!xrtNetServerAccept(scene.server));xrtClearError();
    CHECK(!xrtNetServerListener(scene.server,0));xrtClearError();
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    for(;;){bool all=true;
        for(size_t step=0;step<scene.count;++step){Node* node=&scene.nodes[(step+1)%scene.count];
            if(!node->prep||node->ready)continue;
            xrtownershipprepareresult result=node->prep->Prepare(node->ref.Data,&scene);
            CHECK(result!=XRT_OWNERSHIP_PREPARE_FAILED);node->ready=result==XRT_OWNERSHIP_PREPARE_READY;all=all&&node->ready;}
        if(all)break;
        CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
    if(pending)CHECK(xrtFutureState(scene.future)==XFUTURE_CLOSED);
    freeze(&scope);
    for(size_t i=0;i<scene.count;++i){Node* n=&scene.nodes[i];
        size_t kind=n->ref.Ops==xrtNetServerBindingOwnershipQuiescenceV1Get()->Ops?0:
            n->ref.Ops==xrtNetServerFutureOwnershipQuiescenceV1Get()->Ops?1:2;
        if(kind<2&&!scene.races[kind]){CHECK(n->adapter->Hold(n->ref.Data)&&n->adapter->Hold(n->ref.Data));scene.races[kind]=n;}}
    CHECK(scene.races[0]&&(!pending||scene.races[1])&&xrtOwnershipScopeEnd(&scope));
    proof=snapshot(&scene,&scope);examine(&scene,proof,true);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<scene.count;++i){Node* n=&scene.nodes[i];if(n->prep)CHECK(n->prep->Ready(n->ref.Data));n->adapter->Clear(n->ref.Data,&scene);}
    CHECK(xrtOwnershipScopeEnd(&scope));
    for(size_t i=0;i<scene.count;++i)if(scene.nodes[i].adapter->Finish)CHECK(scene.nodes[i].adapter->Finish(scene.nodes[i].ref.Data,&scene));
    CHECK(xrtNetEngineTryDestroy(scene.engine)==XNET_RETIRE_READY);xrtNetServerDestroy(scene.server);xrtFutureDestroy(scene.future);
    for(size_t i=scene.count;i;--i){Node* n=&scene.nodes[i-1];if(n!=scene.races[0]&&n!=scene.races[1])n->adapter->Drop(n->ref.Data);}
    for(size_t i=0;i<2;++i)if(scene.races[i])concurrentDrop(scene.races[i]);
    balanced(before);++graphs;
}
static void closedShell(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene scene={0};create(&scene,backend);
    CHECK(xrtNetServerClose(scene.server));CHECK(testTcpServerEngineDestroy(scene.engine));
    /* Engine allocation is now genuinely freed; keep only the Server shell. */
    #if defined(XRT_FEATURE_NET_TCP_SERVER_SYNC)
    CHECK(!xrtNetServerAcceptWait(scene.server,xrtDeadlineAfter(0),NULL)&&xrtErrorKind(xrtGetError())==XERR_CLOSED);xrtClearError();
    #endif
    xfuture* future=xrtNetServerAcceptAsync(scene.server);CHECK(future&&xrtFutureState(future)==XFUTURE_CLOSED);
    xrtFutureDestroy(future);xrtNetServerDestroy(scene.server);balanced(before);
}
typedef struct CloseCancel {xnetserver* server;xfuture* future;xatomic32 ready,go;} CloseCancel;
static int32 closeRace(ptr data)
{
    CloseCancel* race=data;xrtAtomic32FetchAdd(&race->ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&race->go,XMEMORY_ACQUIRE))xrtThreadYield();
    CHECK(xrtNetServerClose(race->server));xrtNetServerDestroy(race->server);return 0;
}
static int32 cancelRace(ptr data)
{
    CloseCancel* race=data;xrtAtomic32FetchAdd(&race->ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&race->go,XMEMORY_ACQUIRE))xrtThreadYield();
    bool cancelled=xrtFutureCancel(race->future);
    CHECK(cancelled||xrtFutureState(race->future)!=XFUTURE_PENDING);
    xrtFutureDestroy(race->future);xrtClearError();return 0;
}
static void closeCancelRaces(xnetportbackend backend)
{
    for(unsigned iteration=0;iteration<8;++iteration){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene scene={0};create(&scene,backend);
        scene.future=xrtNetServerAcceptAsync(scene.server);CHECK(scene.future);CloseCancel race={0};
        race.server=xrtNetServerRef(scene.server);race.future=xrtFutureRef(scene.future);CHECK(race.server&&race.future);
        xrtAtomic32Init(&race.ready,0);xrtAtomic32Init(&race.go,0);
        /* Only real binding/runtime/activation and thread-owned references
         * remain. Main's old alias is never used after this creator Drop. */
        xrtNetServerDestroy(scene.server);scene.server=NULL;
        xthread* a=xrtThreadCreate(closeRace,&race,0);xthread* b=xrtThreadCreate(cancelRace,&race,0);CHECK(a&&b);
        xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
        while(xrtAtomic32Load(&race.ready,XMEMORY_ACQUIRE)!=2){CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
        xrtAtomic32Store(&race.go,1,XMEMORY_RELEASE);
        CHECK(xrtFutureWaitFor(scene.future,UINT64_C(10000000))==XWAIT_OK);
        CHECK(xrtFutureState(scene.future)==XFUTURE_CLOSED||xrtFutureState(scene.future)==XFUTURE_CANCELLED);
        CHECK(xrtThreadWaitFor(a,UINT64_C(10000000))==XWAIT_OK&&xrtThreadExitCode(a)==0);CHECK(xrtThreadWaitFor(b,UINT64_C(10000000))==XWAIT_OK&&xrtThreadExitCode(b)==0);
        xrtThreadDestroy(a);xrtThreadDestroy(b);xrtFutureDestroy(scene.future);CHECK(testTcpServerEngineDestroy(scene.engine));balanced(before);
    }
}
static void completed(xfuture* future)
{CHECK(future&&xrtFutureWaitFor(future,UINT64_C(10000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);}
static void exchange(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Scene scene={0};create(&scene,backend);
    for(size_t endpoint=0;endpoint<2;++endpoint){xnetaddr address;CHECK(xrtNetServerLocal(scene.server,endpoint,&address));
        scene.future=xrtNetServerAcceptAsync(scene.server);CHECK(scene.future);
        scene.client=xrtNetStreamConnect(scene.engine,&address,1,NULL,NULL,NULL);CHECK(scene.client);completed(scene.future);
        xnetstream* borrowed=xrtFutureValue(scene.future);CHECK(borrowed&&!xrtNetStreamData(borrowed));
        xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&scene,&scope);examine(&scene,proof,false);
        xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
        xnetstream* peer=xrtNetStreamRef(borrowed);CHECK(peer);xrtFutureDestroy(scene.future);scene.future=NULL;
        xfuture* open=xrtNetStreamWaitAsync(scene.client,XNET_STREAM_WAIT_OPEN);completed(open);xrtFutureDestroy(open);
        CHECK(xrtNetStreamSend(scene.client,"a\0b",3)==XNET_RESULT_OK);size_t received=0;
        while(received<3){xfuture* read=xrtNetStreamRecvAsync(peer,3-received);completed(read);xbytesview view=xrtNetBytesView(xrtFutureValue(read));
            CHECK(view.Size&&view.Size<=3-received&&!memcmp(view.Data,"a\0b"+received,view.Size));received+=view.Size;xrtFutureDestroy(read);}
        CHECK(xrtNetStreamAbort(scene.client)&&xrtNetStreamAbort(peer));xrtNetStreamDestroy(scene.client);scene.client=NULL;xrtNetStreamDestroy(peer);++transfers;}
    CHECK(xrtNetServerClose(scene.server));xrtNetServerDestroy(scene.server);CHECK(testTcpServerEngineDestroy(scene.engine));balanced(before);
}
static void noCallbacks(xnetserver* server,ptr data){(void)server;(void)data;}
static void opaque(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Backend=backend;ec.Workers=1;
    xnetengine* engine=xrtNetEngineCreate(&ec);CHECK(engine&&xrtNetEngineStart(engine));xnetserverconfig config;xrtNetServerConfigInit(&config);
    CHECK(xrtNetAddrLoopback(&config.Listen.Address,XNET_FAMILY_IPV4,0));xnetserverevents events={0};events.Close=noCallbacks;
    xnetserver* server=xrtNetServerStart(engine,&config,&events,NULL,NULL);CHECK(server);xrtownershipscope scope={0};idle(engine,&scope);
    const xrtownershippreparationv1* prep=(const xrtownershippreparationv1*)engine;size_t count=71;
    CHECK(!xrtNetServerOwnershipAdapterV1(xrtNetServerOwnership(server),&prep)&&prep==(const xrtownershippreparationv1*)engine);
    CHECK(!xrtNetServerOwnership(server).Ops->Count(server,&count)&&count==71);++refusals;
    CHECK(xrtOwnershipScopeEnd(&scope));CHECK(xrtNetServerClose(server));xrtNetServerDestroy(server);CHECK(testTcpServerEngineDestroy(engine));balanced(before);
}
typedef struct Gate {xatomic32 entered,open;} Gate;
static void gateTask(xnetworker* worker,ptr data)
{(void)worker;Gate* gate=data;xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    while(!xrtAtomic32Load(&gate->open,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}}
static void allocations(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Backend=backend;ec.Workers=1;ec.NodeCacheBytes=0;
    xnetengine* engine=xrtNetEngineCreate(&ec);CHECK(engine&&xrtNetEngineStart(engine));xnetserverconfig config;xrtNetServerConfigInit(&config);
    CHECK(xrtNetAddrLoopback(&config.Listen.Address,XNET_FAMILY_IPV4,0));xnetlistenconfig second=config.Listen;config.Additional=&second;config.AdditionalCount=1;
    bool converged=false;
    for(size_t budget=0;budget<128;++budget){Gate gate;xrtAtomic32Init(&gate.entered,0);xrtAtomic32Init(&gate.open,0);
        CHECK(xrtNetEnginePost(engine,0,gateTask,&gate));xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
        while(!xrtAtomic32Load(&gate.entered,XMEMORY_ACQUIRE)){CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
        CHECK(xrtMemDebugFailAfter(budget));xnetserver* server=xrtNetServerStart(engine,&config,NULL,NULL,NULL);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++oomBudgets;if(hit)++oomFailures;xrtClearError();
        xrtAtomic32Store(&gate.open,1,XMEMORY_RELEASE);
        if(server){CHECK(!hit&&xrtNetServerClose(server));xrtNetServerDestroy(server);converged=true;}
        xrtownershipscope scope={0};for(;;){idle(engine,&scope);bool clean=!engine->RuntimeOwnerCount&&!xrtAtomic32Load(&engine->LiveObjects,XMEMORY_ACQUIRE);
            CHECK(xrtOwnershipScopeEnd(&scope));if(clean)break;CHECK(!xrtDeadlineExpired(due));xrtThreadYield();}
        if(converged)break;
        CHECK(hit);}
    CHECK(converged);CHECK(testTcpServerEngineDestroy(engine));balanced(before);
    Scene scene={0};create(&scene,backend);converged=false;
    for(size_t budget=0;budget<128;++budget){CHECK(xrtMemDebugFailAfter(budget));xfuture* future=xrtNetServerAcceptAsync(scene.server);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++oomBudgets;if(hit)++oomFailures;
        if(future){CHECK(!hit&&xrtFutureCancel(future));xrtFutureDestroy(future);converged=true;}
        else CHECK(hit);
        xrtClearError();xnetserverstats stats;CHECK(xrtNetServerStats(scene.server,&stats)&&!stats.AcceptWaiters);
        if(converged)break;}
    CHECK(converged&&xrtNetServerClose(scene.server));xrtNetServerDestroy(scene.server);CHECK(testTcpServerEngineDestroy(scene.engine));balanced(before);
}
int main(int argc,char** argv)
{
    xnetportbackend backend=XNET_PORT_SELECT;if(argc>1&&!strcmp(argv[1],"native")){
#ifdef _WIN32
        backend=XNET_PORT_IOCP;
#elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
#endif
    }
    CHECK(xrtMemDebugEnable(true));opaque(backend);allocations(backend);exchange(backend);closedShell(backend);closeCancelRaces(backend);
    for(unsigned i=0;i<3;++i){graphCase(backend,false);graphCase(backend,true);}
    testMemoryDebugDrain("Server physical graphs and callbacks converge");
    printf("[PASS] Server owned graph backend=%d graphs=%u refusals=%u OOM-budgets=%u OOM-failures=%u NUL-transfers=%u drop-races=%u no fabricated roots, balanced\n",
        (int)backend,graphs,refusals,oomBudgets,oomFailures,transfers,dropRaces);return 0;
}
