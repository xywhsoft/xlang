/* Real managed resource, parent owners and native TCP graph; loopback only. */
#include "tcp-server-resource-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_execution_tcp_internal.h"
#include "src/runtime/xl6_execution_tcp_server_internal.h"
#include "src/runtime/xl6_execution_net_engine.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
typedef XL6_ExecutionResourceV1 Resource;
enum {CAPACITY=80};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* prepare;const xrtownershipfinishv1* finish;bool ready,done;} Node;
typedef struct Graph {XL6_ExecutionSourceV1* source;Resource *engine,*server,*client,*peer,*pending;
    Node nodes[CAPACITY];size_t count;bool pinned,busy;} Graph;
static XL6_OwnershipCatalog* catalog;
static unsigned graphs,ordinary,budgets,failures,abandoned,deferred;
static bool serverLocal(Resource* r,xnetaddr* address)
{
    XL6_TcpServerScopeV1 scope={0};if(!xl6ExecutionTcpServerEnterInternalV1(r,false,&scope))return false;
    bool result=xrtNetServerLocal(scope.server,0,address);xl6ExecutionTcpServerLeaveInternalV1(&scope);return result;
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void freeze(xrtownershipscope* scope)
{XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}}
static bool admit(xrtownershipref ref,void* raw)
{
    Graph* graph=raw;const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;
    const xrtownershipfinishv1* finish=NULL;
    XL6_OwnershipAdmissionV1 result=xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&prepare,&finish);
    if(result==XL6_OWNERSHIP_ADMISSION_BUSY)graph->busy=true;
    return result==XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
static xrtownershipsnapshot* snapshot(Graph* graph,xrtownershipscope* scope)
{
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        freeze(scope);graph->busy=false;
        xrtownershipref anchors[CAPACITY+6]={xl6ExecutionSourceOwnershipV1(graph->source),xl6ExecutionResourceOwnershipV1(graph->engine),
            xl6ExecutionResourceOwnershipV1(graph->server),xl6ExecutionResourceOwnershipV1(graph->client),xl6ExecutionResourceOwnershipV1(graph->peer)};
        /* Engine's original caller owner was really released. Each child
         * owns a separate parent reference, enumerated by its actual family. */
        xrtownershipref slots[CAPACITY+5]={anchors[0],anchors[2],anchors[3],anchors[4]};size_t a=5,s=4;
        if(graph->pending){anchors[a++]=xl6ExecutionResourceOwnershipV1(graph->pending);slots[s++]=anchors[a-1];}
        if(graph->pinned)for(size_t i=0;i<graph->count;++i){anchors[a++]=graph->nodes[i].ref;slots[s++]=graph->nodes[i].ref;}
        xrtownershipsnapshot* proof=NULL;
        if(xrtOwnershipSnapshotCreate(anchors,a,slots,s,admit,graph,&proof))return proof;
        CHECK(graph->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(scope));CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();
    }
}
static void discover(Graph* graph,xrtownershipsnapshot* proof)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode item;
        CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);size_t j=0;
        while(j<graph->count&&(graph->nodes[j].ref.Data!=item.Reference.Data||graph->nodes[j].ref.Ops!=item.Reference.Ops))++j;
        if(j<graph->count)continue;
        CHECK(graph->count<CAPACITY);Node* n=&graph->nodes[graph->count++];n->ref=item.Reference;
        CHECK(xl6OwnershipCatalogResolveV1(n->ref,catalog,&n->adapter,&n->prepare,&n->finish));
        CHECK(n->adapter->Hold(n->ref.Data)&&n->adapter->Claim(n->ref.Data,graph));
    }
    graph->pinned=true;
}
static void createWithEngineOptions(Graph* graph,xnetportbackend backend,bool managedAccept,const xnetengineconfig* requested)
{
    graph->source=xl6ExecutionSourceCreateV1();CHECK(graph->source);
    xnetengineconfig config;if(requested)config=*requested;else xrtNetEngineConfigInit(&config);
    config.Workers=1;config.Backend=backend;
    graph->engine=xl6ExecutionNetEngineCreateV1(graph->source,&config);CHECK(graph->engine&&xl6ExecutionNetEngineStartV1(graph->engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    xnetserverconfig serverConfig;xrtNetServerConfigInit(&serverConfig);serverConfig.Listen=listen;
    xnetlistenconfig second=listen;serverConfig.Additional=&second;serverConfig.AdditionalCount=1;
    graph->server=xl6ExecutionTcpServerCreateV1(graph->source,graph->engine,&serverConfig);CHECK(graph->server);
    xnetaddr address;CHECK(serverLocal(graph->server,&address));
    xfuture* future=NULL;XL6_TcpScopeV1 scope={0};
    if(managedAccept){
        graph->peer=xl6ExecutionTcpServerPrepareAcceptV1(graph->source,graph->server);CHECK(graph->peer);
        CHECK(!xl6ExecutionTcpEnterInternalV1(graph->peer,XL6_EXECUTION_TCP_STREAM_V1,false,&scope));
        xrtClearError();CHECK(!scope.execution.resource);
        future=xl6ExecutionTcpServerAcceptSourceForV1(graph->peer,graph->server);CHECK(future&&xrtFutureState(future)==XFUTURE_PENDING);
        xfuture* same=xl6ExecutionTcpServerAcceptSourceForV1(graph->peer,graph->server);CHECK(same==future);xrtFutureDestroy(same);
    }else{
        XL6_TcpServerScopeV1 accepting={0};CHECK(xl6ExecutionTcpServerEnterInternalV1(graph->server,false,&accepting));
        future=xrtNetServerAcceptAsync(accepting.server);CHECK(future);xl6ExecutionTcpServerLeaveInternalV1(&accepting);
    }
    graph->client=xl6ExecutionTcpConnectV1(graph->source,graph->engine,&address,0,&listen.Stream);CHECK(graph->client);
    CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
    if(managedAccept){
        CHECK(xl6ExecutionTcpServerPromoteAcceptV1(graph->peer));
        CHECK(!xl6ExecutionTcpServerPromoteAcceptV1(graph->peer));xrtClearError();
    }else graph->peer=xl6ExecutionTcpAdoptV1(graph->source,graph->engine,xrtFutureValue(future));
    CHECK(graph->peer);xrtFutureDestroy(future);
    XL6_ExecutionResourceSnapshotV1 state={.size=sizeof(state)};
    CHECK(xl6ExecutionResourceSnapshotV1Get(graph->engine,&state)&&state.owners==4);
    xl6ExecutionResourceReleaseV1(graph->engine);
    CHECK(xl6ExecutionResourceSnapshotV1Get(graph->engine,&state)&&state.owners==3&&!state.closing);
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);int phase=0;
    while(xl6ExecutionTcpStateV1(graph->client,&phase)&&phase!=XNET_STREAM_OPEN){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    CHECK(phase==XNET_STREAM_OPEN);
}
static void create(Graph* graph,xnetportbackend backend,bool managedAccept)
{createWithEngineOptions(graph,backend,managedAccept,NULL);}
static void roundTrip(Graph* graph)
{
    XL6_TcpScopeV1 scope={0};CHECK(xl6ExecutionTcpEnterInternalV1(graph->client,XL6_EXECUTION_TCP_STREAM_V1,true,&scope));
    CHECK(xrtNetStreamSend(scope.stream,"abc\0def",7)==XNET_RESULT_OK);xl6ExecutionTcpLeaveInternalV1(&scope);
    CHECK(xl6ExecutionTcpEnterInternalV1(graph->peer,XL6_EXECUTION_TCP_STREAM_V1,true,&scope));
    xfuture* read=xrtNetStreamRecvAsync(scope.stream,7);CHECK(read);xl6ExecutionTcpLeaveInternalV1(&scope);
    CHECK(xrtFutureWaitFor(read,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(read)==XFUTURE_RESOLVED);
    xbytesview bytes=xrtNetBytesView(xrtFutureValue(read));CHECK(bytes.Size==7&&memcmp(bytes.Data,"abc\0def",7)==0);xrtFutureDestroy(read);
    CHECK(!xl6ExecutionTcpEnterInternalV1(graph->server,XL6_EXECUTION_TCP_STREAM_V1,false,&scope)&&xrtErrorKind(xrtGetError())==XERR_TYPE);
    xrtClearError();CHECK(!scope.execution.resource&&!scope.listener&&!scope.stream);
}
static void dropOrdinary(Graph* graph)
{
    xl6ExecutionResourceReleaseV1(graph->pending);
    xl6ExecutionResourceReleaseV1(graph->client);xl6ExecutionResourceReleaseV1(graph->peer);xl6ExecutionResourceReleaseV1(graph->server);
    CHECK(xl6ExecutionSourceCloseV1(graph->source));XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&graph->source);CHECK(result.ok);
        if(result.ready)break;
        CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
}
static void collect(Graph* graph,unsigned order)
{
    xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(graph,&scope);discover(graph,proof);
    xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){bool ready=true;
        for(size_t step=0;step<graph->count;++step){size_t i=order&1?graph->count-1-step:step;Node* n=&graph->nodes[i];
            if(!n->prepare||n->ready)continue;
            proof=snapshot(graph,&scope);discover(graph,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
            xrtownershipprepareresult result=n->prepare->Prepare(n->ref.Data,graph);CHECK(result!=XRT_OWNERSHIP_PREPARE_FAILED);
            n->ready=result==XRT_OWNERSHIP_PREPARE_READY;ready=ready&&n->ready;}
        if(ready)break;
        CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    proof=snapshot(graph,&scope);discover(graph,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<graph->count;++i){Node* n=&graph->nodes[i];CHECK(!n->prepare||n->prepare->Ready(n->ref.Data));n->adapter->Clear(n->ref.Data,graph);}
    CHECK(xrtOwnershipScopeEnd(&scope));until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){bool done=true;
        for(size_t step=0;step<graph->count;++step){size_t i=order&2?graph->count-1-step:step;Node* n=&graph->nodes[i];if(n->done)continue;
            if(n->finish){xrtownershipfinishresult result=n->finish->Step(n->ref.Data,graph);CHECK(result!=XRT_OWNERSHIP_FINISH_FAILED);n->done=result==XRT_OWNERSHIP_FINISH_READY;}
            else n->done=!n->adapter->Finish||n->adapter->Finish(n->ref.Data,graph);
            done=done&&n->done;}
        if(done)break;
        CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    xl6ExecutionResourceReleaseV1(graph->server);xl6ExecutionResourceReleaseV1(graph->client);xl6ExecutionResourceReleaseV1(graph->peer);
    xl6ExecutionResourceReleaseV1(graph->pending);
    XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&graph->source);CHECK(result.ready&&result.ok&&!graph->source);
    for(size_t step=0;step<graph->count;++step){size_t i=order&4?graph->count-1-step:step;graph->nodes[i].adapter->Drop(graph->nodes[i].ref.Data);}
}
static void pending(Graph* graph,unsigned state)
{
    if(!state)return;
    graph->pending=xl6ExecutionTcpServerPrepareAcceptV1(graph->source,graph->server);CHECK(graph->pending);
    if(state==2){xfuture* future=xl6ExecutionTcpServerAcceptSourceForV1(graph->pending,graph->server);CHECK(future&&xrtFutureState(future)==XFUTURE_PENDING);
        xrtFutureDestroy(future);}
}
static void abandonedAccept(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true);
    Resource* carrier=xl6ExecutionTcpServerPrepareAcceptV1(graph.source,graph.server);CHECK(carrier);
    xfuture* future=xl6ExecutionTcpServerAcceptSourceForV1(carrier,graph.server);CHECK(future);
    xnetaddr address;CHECK(serverLocal(graph.server,&address));
    xnetstreamconfig config;xrtNetStreamConfigInit(&config);config.ReadMode=XNET_STREAM_READ_PROBE;
    Resource* client=xl6ExecutionTcpConnectV1(graph.source,graph.engine,&address,0,&config);CHECK(client);
    CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
    xnetstream* observer=xrtNetStreamRef(xrtFutureValue(future));CHECK(observer);
    /* Keep both the Engine and an actual native observer alive: dropping the
     * unpromoted language carrier must close this unobserved accepted result,
     * not wait for eventual Engine destruction to hide an orphan connection. */
    xl6ExecutionResourceReleaseV1(carrier);xrtFutureDestroy(future);
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(xrtNetStreamState(observer)!=XNET_STREAM_CLOSED){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    xrtNetStreamDestroy(observer);xl6ExecutionResourceReleaseV1(client);dropOrdinary(&graph);balanced(&before);++abandoned;
}
static void construction(xnetportbackend backend)
{
    for(unsigned kind=0;kind<5;++kind)for(unsigned budget=0;budget<16;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true);
        Resource* carrier=NULL;xfuture* future=NULL;XL6_TcpScopeV1 borrowed={0};
        xnetlistenconfig config;xrtNetListenConfigInit(&config);config.Stream.ReadMode=XNET_STREAM_READ_PROBE;
        CHECK(xrtNetAddrLoopback(&config.Address,XNET_FAMILY_IPV4,0));
        xnetserverconfig serverConfig;xrtNetServerConfigInit(&serverConfig);serverConfig.Listen=config;
        xnetaddr address;CHECK(serverLocal(graph.server,&address));
        if(kind==2)CHECK(xl6ExecutionTcpEnterInternalV1(graph.peer,XL6_EXECUTION_TCP_STREAM_V1,false,&borrowed));
        if(kind==4){carrier=xl6ExecutionTcpServerPrepareAcceptV1(graph.source,graph.server);CHECK(carrier);}
        CHECK(xrtMemDebugFailAfter(budget));++budgets;
        switch(kind){
            case 0:carrier=xl6ExecutionTcpServerCreateV1(graph.source,graph.engine,&serverConfig);break;
            case 1:carrier=xl6ExecutionTcpConnectV1(graph.source,graph.engine,&address,0,&config.Stream);break;
            case 2:carrier=xl6ExecutionTcpAdoptV1(graph.source,graph.engine,borrowed.stream);break;
            case 3:carrier=xl6ExecutionTcpServerPrepareAcceptV1(graph.source,graph.server);break;
            case 4:future=xl6ExecutionTcpServerAcceptSourceForV1(carrier,graph.server);break;
        }
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(kind==4?!future:!carrier){CHECK(failed);++failures;}else CHECK(!failed);
        xrtClearError();if(borrowed.execution.resource)xl6ExecutionTcpLeaveInternalV1(&borrowed);
        xrtFutureDestroy(future);xl6ExecutionResourceReleaseV1(carrier);
        dropOrdinary(&graph);balanced(&before);
    }
}
/* Actual single-implementation source boundary, with no production test hook.
 * Cancellation after native unlink stays pending and may resolve successfully.
 * The source controller must keep the carrier until that delivery is observed. */
static void deferredAccept(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true);
    Resource* carrier=xl6ExecutionTcpServerPrepareAcceptV1(graph.source,graph.server);CHECK(carrier);
    xfuture* future=xl6ExecutionTcpServerAcceptSourceForV1(carrier,graph.server);CHECK(future&&xrtFutureState(future)==XFUTURE_PENDING);
    XL6_TcpServerScopeV1 scope={0};CHECK(xl6ExecutionTcpServerEnterInternalV1(graph.server,true,&scope));
    CHECK(__xrtNetServerEnter(scope.server,false));xrtownershipscope mutation={0};CHECK(xrtOwnershipMutationBegin(&mutation));
    __xrtSpinLock(&scope.server->Lock);
    __xrt_net_server_wait* waiter=scope.server->WaitHead;CHECK(waiter&&__xrtNetServerWaitRemove(scope.server,waiter));
    __xrtSpinUnlock(&scope.server->Lock);CHECK(xrtOwnershipScopeEnd(&mutation));__xrtNetServerLeave(scope.server);
    xnetaddr address;CHECK(xrtNetServerLocal(scope.server,0,&address));
    xnetstreamconfig config;xrtNetStreamConfigInit(&config);config.ReadMode=XNET_STREAM_READ_PROBE;
    Resource* client=xl6ExecutionTcpConnectV1(graph.source,graph.engine,&address,0,&config);CHECK(client);
    xnetstream* peer=NULL;XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!(peer=xrtNetServerAccept(scope.server))){CHECK(!xrtGetError()&&!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    xl6ExecutionTcpServerLeaveInternalV1(&scope);xnetstream* observer=xrtNetStreamRef(peer);CHECK(observer);
    xcancel* cancel=xrtFutureCancelToken(future);CHECK(cancel);
    /* A real inspection pin (not a user owner) keeps the record observable
     * while retirement proceeds. Freeze + quiescence witnesses the completed
     * cancellation attempt, not a timing window immediately after the flag. */
    const xrtownershipadapterv1* carrierAdapter=NULL;const xrtownershippreparationv1* carrierPrepare=NULL;
    const xrtownershipfinishv1* carrierFinish=NULL;xrtownershipscope inspection={0};freeze(&inspection);
    CHECK(xl6OwnershipCatalogProbeV1(xl6ExecutionResourceOwnershipV1(carrier),catalog,
        &carrierAdapter,&carrierPrepare,&carrierFinish)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    CHECK(carrierAdapter->Hold(carrier)&&xrtOwnershipScopeEnd(&inspection));
    xl6ExecutionResourceReleaseV1(carrier);until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtCancelRequested(cancel)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    CHECK(xrtFutureState(future)==XFUTURE_PENDING);
    for(;;){freeze(&inspection);
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const xrtownershipfinishv1* finish=NULL;
        XL6_OwnershipAdmissionV1 admission=xl6OwnershipCatalogProbeV1(xl6ExecutionResourceOwnershipV1(carrier),catalog,&adapter,&prepare,&finish);
        if(admission!=XL6_OWNERSHIP_ADMISSION_BUSY){
            CHECK(admission==XL6_OWNERSHIP_ADMISSION_ADMITTED&&carrier->source==graph.source);
            XL6_ExecutionResourceSnapshotV1 observed={.size=sizeof(observed)};
            CHECK(xl6ExecutionResourceSnapshotV1Get(carrier,&observed)&&observed.nativePresent);
            CHECK(xrtOwnershipScopeEnd(&inspection));break;
        }
        CHECK(xrtOwnershipScopeEnd(&inspection)&&!xl6DeadlineExpiredV1(until));xrtThreadYield();
    }
    XL6_ExecutionSourceSnapshotV1 sourceState={.size=sizeof(sourceState)};
    CHECK(xl6ExecutionSourceSnapshotV1Get(graph.source,&sourceState)&&sourceState.resources==6&&!sourceState.failed);
    __xrtNetServerWaitFinish(waiter,peer,false);xrtCancelDestroy(cancel);xrtFutureDestroy(future);
    carrierAdapter->Drop(carrier);
    until=xl6DeadlineAfterMillisecondsV1(5000);
    while(xrtNetStreamState(observer)!=XNET_STREAM_CLOSED){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    xrtNetStreamDestroy(observer);xl6ExecutionResourceReleaseV1(client);dropOrdinary(&graph);balanced(&before);++deferred;
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
    xl6_error_result error=xl6ErrorScopeEnd(&scope);CHECK(error.ok);xrtErrorFree(error.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "tcp-server-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&strcmp(argv[1],"native")==0){
        #if defined(_WIN32) || defined(_WIN64)
        backend=XNET_PORT_IOCP;
        #elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned state=0;state<3;++state)for(unsigned order=0;order<9;++order){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};
        create(&graph,backend,(order&1)!=0);roundTrip(&graph);pending(&graph,state);
        if(order<8){collect(&graph,order);++graphs;}else{dropOrdinary(&graph);++ordinary;}
        balanced(&before);
    }
    abandonedAccept(backend);deferredAccept(backend);construction(backend);CHECK(failures>0&&failures<budgets);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("managed Server resources: graphs=%u ordinary=%u abandoned=%u deferred=%u allocation-budgets=%u failures=%u; parent owners, exact scoped kinds, NUL roundtrip; balanced backend=%d\n",
        graphs,ordinary,abandoned,deferred,budgets,failures,(int)backend);
    return 0;
}
