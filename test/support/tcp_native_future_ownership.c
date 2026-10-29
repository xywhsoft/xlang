/* XRT 40704d47: only include and pinned millisecond deadline adaptation. */
#define XRT_IMPLEMENTATION
#include "release/include/xl_clock.h"
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool condition,const char* message)
{if(!condition){fprintf(stderr,"[FAIL] %s\n",message);exit(1);}}
#define testMemoryDebugDrain(Message) testRequire(xrtMemDebugReset(),(Message))
#define CHECK(C) do { if (!(C)) { fprintf(stderr,"line %u: ",(unsigned)__LINE__);testRequire(false,#C); } } while(0)
enum {CAPACITY=64};
typedef struct Node {
    xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prepare;
    bool ready;
} Node;
typedef struct Graph {
    xnetengine* engine;xnetlistener* listener;xnetstream* client;
    xfuture* futures[4];size_t futureCount;
    Node nodes[CAPACITY];size_t count;bool busy,pinned,wrong;
    Node* race;
} Graph;
static unsigned graphCases,rootCases,dropRaces,budgets,failures;
typedef struct DropRace { Node* node;xatomic32 ready,go; } DropRace;
static int32 concurrentDrop(ptr raw)
{
    DropRace* race=raw;
    (void)xrtAtomic32FetchAdd(&race->ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&race->go,XMEMORY_ACQUIRE))xrtThreadYield();
    race->node->adapter->Drop(race->node->ref.Data);
    return 0;
}
static void freeze(xrtownershipscope* scope)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static const xrtownershipadapterv1* resolve(Graph* graph,xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    const xrtownershipquiescencev1* quiet[7]={xrtNetEngineOwnershipQuiescenceV1Get(),xrtNetStreamOwnershipQuiescenceV1Get(),
        xrtNetListenerOwnershipQuiescenceV1Get(),xrtNetBufPoolOwnershipQuiescenceV1Get(),xrtNetTcpFutureOwnershipQuiescenceV1Get(),
        xrtFutureOwnershipQuiescenceV1Get(),xrtCancelOwnershipQuiescenceV1Get()};
    for(size_t i=0;i<7;++i)if(ref.Ops==quiet[i]->Ops&&quiet[i]->Busy(ref.Data)){graph->busy=true;return NULL;}
    const xrtownershipquiescencev1* watch=xrtCancelWatchOwnershipQuiescenceV1Get();
    if(ref.Ops==watch->Ops&&watch->Busy(ref.Data)){graph->busy=true;return NULL;}
    const xnetruntimeownershipv1* runtimes[2]={xrtNetStreamRuntimeOwnershipV1Get(),xrtNetListenerRuntimeOwnershipV1Get()};
    const xfutureproducerownershipv1* producers[1]={xrtNetTcpFutureProducerPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[1]={xrtNetTcpFutureCancelPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[2]={xrtNetTcpFutureBytesPayloadPolicyV1Get(),xrtNetTcpFutureStreamPayloadPolicyV1Get()};
    xfutureproducerownershipv1 copied=*producers[0];if(graph->wrong)producers[0]=&copied;
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.PayloadPolicies=payloads;admission.PayloadPolicyCount=2;
    admission.ProducerPolicies=producers;admission.ProducerPolicyCount=1;
    admission.CancelWatchPolicies=cancels;admission.CancelWatchPolicyCount=1;
    const xrtownershipadapterv1* a=xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtimes,2,preparation);
    if(!a)a=xrtNetStreamOwnershipAdapterV1(ref,preparation);
    if(!a)a=xrtNetListenerOwnershipAdapterV1(ref,preparation);
    if(!a)a=xrtNetBufPoolOwnershipAdapterV1(ref,preparation);
    if(!a)a=xrtNetTcpFutureOwnershipAdapterV1(ref,preparation);
    if(!a)a=xrtFutureOwnershipAdapterV4(ref,&admission,preparation);
    if(!a)a=xrtCancelOwnershipAdapterV2(ref,cancels,1,preparation);
    if(!a)a=xrtCancelWatchOwnershipAdapterV1(ref,cancels,1,preparation);
    if(!a)a=xrtNetBytesOwnershipAdapterV1(ref);
    if(!a)a=xrtErrorOwnershipAdapterV1(ref);
    return a;
}
static bool admit(xrtownershipref ref,ptr raw)
{const xrtownershippreparationv1* p=NULL;return resolve(raw,ref,&p)!=NULL;}
static xrtownershipsnapshot* snapshot(Graph* graph,bool external,xrtownershipscope* scope)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        freeze(scope);graph->busy=false;
        xrtownershipref anchors[CAPACITY+7],slots[CAPACITY+9];size_t a=0,s=0;
        anchors[a++]=xrtNetEngineOwnership(graph->engine);anchors[a++]=xrtNetListenerOwnership(graph->listener);
        anchors[a++]=xrtNetStreamOwnership(graph->client);
        for(size_t i=0;i<graph->futureCount;++i)anchors[a++]=xrtFutureOwnership(graph->futures[i]);
        for(size_t i=0;i<a;++i)if(!external||i+1<a)slots[s++]=anchors[i];
        if(graph->pinned)for(size_t i=0;i<graph->count;++i){slots[s++]=graph->nodes[i].ref;anchors[a++]=graph->nodes[i].ref;}
        if(graph->race){slots[s++]=graph->race->ref;slots[s++]=graph->race->ref;}
        xrtownershipsnapshot* result=NULL;
        if(xrtOwnershipSnapshotCreate(anchors,a,slots,s,admit,graph,&result))return result;
        CHECK(graph->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(scope));
        CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
    }
}
static void discover(Graph* graph,xrtownershipsnapshot* snapshot)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){
        xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(snapshot,i,&item)&&!item.Reachable);
        size_t j=0;while(j<graph->count&&(graph->nodes[j].ref.Data!=item.Reference.Data||graph->nodes[j].ref.Ops!=item.Reference.Ops))++j;
        if(j<graph->count)continue;
        CHECK(graph->count<CAPACITY);Node* node=&graph->nodes[graph->count++];node->ref=item.Reference;
        node->adapter=resolve(graph,node->ref,&node->prepare);CHECK(node->adapter);
        CHECK(node->adapter->Hold(node->ref.Data)&&node->adapter->Claim(node->ref.Data,graph));
        if(node->prepare)CHECK(!node->adapter->Claim(node->ref.Data,&item));
    }
    graph->pinned=true;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void create(Graph* graph,xnetportbackend backend)
{
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=backend;config.Workers=1;config.NodeCacheBytes=4096;
    graph->engine=xrtNetEngineCreate(&config);CHECK(graph->engine&&xrtNetEngineStart(graph->engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    graph->listener=xrtNetListen(graph->engine,&listen,NULL,NULL,NULL);CHECK(graph->listener);
    xnetaddr address;CHECK(xrtNetListenerLocal(graph->listener,&address));
    graph->client=xrtNetStreamConnect(graph->engine,&address,0,&listen.Stream,NULL,NULL);CHECK(graph->client);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(xrtNetStreamState(graph->client)!=XNET_STREAM_OPEN||!xrtAtomic32Load(&graph->listener->QueuedAccepts,XMEMORY_ACQUIRE)){
        CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void closeOrdinary(Graph* graph)
{
    CHECK(xrtNetStreamAbort(graph->client)&&xrtNetListenerClose(graph->listener));
    xrtNetStreamDestroy(graph->client);xrtNetListenerDestroy(graph->listener);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){xnetretireresult r=xrtNetEngineTryDestroy(graph->engine);if(r==XNET_RETIRE_READY)break;
        CHECK(r==XNET_RETIRE_BUSY&&!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void lifecycle(xnetportbackend backend,unsigned order,unsigned outcome)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend);
    graph.futures[graph.futureCount++]=xrtNetListenerAcceptAsync(graph.listener);CHECK(graph.futures[0]);
    CHECK(xrtFutureWaitFor(graph.futures[0],UINT64_C(5000000))==XWAIT_OK);
    xnetstream* peer=xrtFutureValue(graph.futures[0]);CHECK(peer);
    graph.futures[graph.futureCount++]=xrtNetListenerAcceptAsync(graph.listener);
    graph.futures[graph.futureCount++]=xrtNetStreamWaitAsync(graph.client,XNET_STREAM_WAIT_CLOSE);
    graph.futures[graph.futureCount++]=xrtNetStreamRecvAsync(peer,8);
    for(size_t i=0;i<4;++i)CHECK(graph.futures[i]);
    if(outcome==1){CHECK(xrtNetStreamSend(graph.client,"abc\0def",7)==XNET_RESULT_OK);
        CHECK(xrtFutureWaitFor(graph.futures[3],UINT64_C(5000000))==XWAIT_OK);
        xnetbytes* bytes=xrtFutureValue(graph.futures[3]);CHECK(bytes&&xrtNetBytesView(bytes).Size==7);
        CHECK(memcmp(xrtNetBytesView(bytes).Data,"abc\0def",7)==0);}
    if(outcome==2){CHECK(xrtFutureCancel(graph.futures[3])&&xrtFutureState(graph.futures[3])==XFUTURE_CANCELLED);
        CHECK(xrtNetStreamState(peer)==XNET_STREAM_OPEN);}
    xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&graph,true,&scope);bool rooted=false;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode n;CHECK(xrtOwnershipSnapshotNode(proof,i,&n));rooted=rooted||n.Reachable;
        if(n.Reference.Data==graph.engine)CHECK(n.Reachable==(outcome==0));}
    CHECK(rooted);++rootCases;xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
    proof=snapshot(&graph,false,&scope);CHECK(xrtOwnershipSnapshotNodeCount(proof)==(outcome==1?18u:outcome==2?17u:19u));
    /* Count/Trace sees the actual producer/list/watch references, not an
     * opaque Future leaf or a made-up list-root reference. */
    unsigned pending=0;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode n;CHECK(xrtOwnershipSnapshotNode(proof,i,&n));
        if(n.Reference.Ops==xrtNetTcpFutureOwnershipQuiescenceV1Get()->Ops){CHECK(n.StrongCount==3&&n.InternalCount==3);++pending;}}
    CHECK(pending==(outcome?2u:3u));
    graph.wrong=true;xrtownershipsnapshot* refused=NULL;
    xrtownershipref root=xrtFutureOwnership(graph.futures[1]);CHECK(!xrtOwnershipSnapshotCreate(&root,1,&root,1,admit,&graph,&refused)&&!refused);
    graph.wrong=false;xrtClearError();discover(&graph,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
    CHECK(!xrtNetListenerAcceptAsync(graph.listener));xrtClearError();
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){bool all=true;
        for(size_t step=0;step<graph.count;++step){size_t i=order&1?graph.count-1-step:step;Node* node=&graph.nodes[i];
            if(!node->prepare||node->ready)continue;
            proof=snapshot(&graph,false,&scope);discover(&graph,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
            xrtownershipprepareresult r=node->prepare->Prepare(node->ref.Data,&graph);CHECK(r!=XRT_OWNERSHIP_PREPARE_FAILED);
            node->ready=r==XRT_OWNERSHIP_PREPARE_READY;all=all&&node->ready;}
        if(all)break;
        CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    proof=snapshot(&graph,false,&scope);discover(&graph,proof);xrtOwnershipSnapshotDestroy(proof);
    /* Two actual coordinator pins, not inferred counts. Re-prove the graph
     * including those slots before Clear, then race all three final drops. */
    for(size_t i=0;i<graph.count&&!graph.race;++i)if(graph.nodes[i].ref.Ops==xrtNetTcpFutureOwnershipQuiescenceV1Get()->Ops){
        graph.race=&graph.nodes[i];CHECK(graph.race->adapter->Hold(graph.race->ref.Data));
        CHECK(graph.race->adapter->Hold(graph.race->ref.Data));}
    CHECK(graph.race&&xrtOwnershipScopeEnd(&scope));
    proof=snapshot(&graph,false,&scope);discover(&graph,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<graph.count;++i){Node* n=&graph.nodes[i];CHECK(!n->prepare||n->prepare->Ready(n->ref.Data));n->adapter->Clear(n->ref.Data,&graph);}
    CHECK(xrtOwnershipScopeEnd(&scope));
    for(size_t step=0;step<graph.count;++step){size_t i=order&2?graph.count-1-step:step;Node* n=&graph.nodes[i];
        CHECK(!n->adapter->Finish||n->adapter->Finish(n->ref.Data,&graph));}
    CHECK(xrtNetEngineTryDestroy(graph.engine)==XNET_RETIRE_READY);
    xrtNetStreamDestroy(graph.client);xrtNetListenerDestroy(graph.listener);
    for(size_t i=0;i<4;++i)xrtFutureDestroy(graph.futures[i]);
    for(size_t step=0;step<graph.count;++step){size_t i=order&4?graph.count-1-step:step;
        if(&graph.nodes[i]!=graph.race)graph.nodes[i].adapter->Drop(graph.nodes[i].ref.Data);}
    DropRace race={0};race.node=graph.race;xrtAtomic32Init(&race.ready,0);xrtAtomic32Init(&race.go,0);
    xthread* threads[2]={xrtThreadCreate(concurrentDrop,&race,0),xrtThreadCreate(concurrentDrop,&race,0)};
    CHECK(threads[0]&&threads[1]);deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(xrtAtomic32Load(&race.ready,XMEMORY_ACQUIRE)!=2){CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    xrtAtomic32Store(&race.go,1,XMEMORY_RELEASE);graph.race->adapter->Drop(graph.race->ref.Data);
    for(size_t i=0;i<2;++i){CHECK(xrtThreadWaitFor(threads[i],UINT64_C(5000000))==XWAIT_OK&&xrtThreadExitCode(threads[i])==0);xrtThreadDestroy(threads[i]);}
    ++dropRaces;
    balanced(&before);++graphCases;
}
static void construction(xnetportbackend backend)
{
    for(unsigned listener=0;listener<2;++listener)for(unsigned budget=0;budget<8;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend);
        /* Warm and return a node to the actual cache, then inspect allocation
         * failure on the same caller thread while the worker is frozen. */
        xfuture* warm=xrtNetStreamWaitAsync(graph.client,XNET_STREAM_WAIT_CLOSE);CHECK(warm&&xrtFutureCancel(warm));xrtFutureDestroy(warm);
        xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&graph,false,&scope);xrtOwnershipSnapshotDestroy(proof);
        CHECK(xrtMemDebugFailAfter(budget));++budgets;
        xfuture* future=listener?xrtNetListenerAcceptAsync(graph.listener):xrtNetStreamWaitAsync(graph.client,XNET_STREAM_WAIT_CLOSE);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!future){CHECK(failed);++failures;}else CHECK(!failed);
        xrtClearError();CHECK(xrtOwnershipScopeEnd(&scope));
        if(future){if(xrtFutureState(future)==XFUTURE_PENDING)CHECK(xrtFutureCancel(future));
            else if(listener&&xrtFutureState(future)==XFUTURE_RESOLVED)CHECK(xrtNetStreamAbort(xrtFutureValue(future)));
            xrtFutureDestroy(future);}
        closeOrdinary(&graph);balanced(&before);
    }
}
int main(int argc,char** argv)
{
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&strcmp(argv[1],"native")==0){
        #if defined(_WIN32) || defined(_WIN64)
        backend=XNET_PORT_IOCP;
        #elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
        #endif
    }
    CHECK(xrtMemDebugEnable(true));
    for(unsigned order=0;order<8;++order)for(unsigned outcome=0;outcome<3;++outcome)lifecycle(backend,order,outcome);
    construction(backend);CHECK(failures>0&&failures<budgets);testMemoryDebugDrain("TCP Future graph balance");
    printf("TCP Future graph backend=%d graphs=%u real-external-roots=%u concurrent-final-drops=%u allocation-budgets=%u failures=%u; balanced\n",
        (int)backend,graphCases,rootCases,dropRaces,budgets,failures);return 0;
}
