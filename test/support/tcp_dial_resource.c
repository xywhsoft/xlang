/* Extend the real resource graph fixture, not a duplicated coordinator. Its
 * original TCP/Accept suite also runs, proving the shared carrier regression. */
#define main tcpResourceRegressionMain
#include "tcp_execution_resource.c"
#undef main
#include "src/runtime/xl6_execution_tcp_dial.h"
#include "src/runtime/xl6_execution_net_resolver.h"
static unsigned dialGraphs,dialRoundTrips,dialBudgets,dialFailures,dialAbandoned,dialFrozenProofs;
static bool verifyDialPolicies(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** finish)
{
    /* Inspect the ACTUAL sealed catalog population during resolution of a
     * real graph node. This observer grants no graph capability or edge. */
    (void)ref;(void)adapter;(void)preparation;(void)finish;
    bool runtime=false,producer=false,watch=false,done=false;
    for(size_t i=0;i<policies->NetRuntimeCount;++i)runtime|=policies->NetRuntime[i]==xrtNetDialRuntimeOwnershipV1Get();
    for(size_t i=0;i<policies->ProducerCount;++i)producer|=policies->Producer[i]==xrtNetDialFutureProducerPolicyV1Get();
    for(size_t i=0;i<policies->CancelWatchCount;++i)watch|=policies->CancelWatch[i]==xrtNetDialFutureCancelPolicyV1Get();
    for(size_t i=0;i<policies->DialDoneCount;++i)done|=policies->DialDone[i]==xrtNetDialFutureDialPolicyV1Get();
    CHECK(runtime&&producer&&watch&&done);return false;
}
static const XL6_OwnershipDomainV1 policyObserver={sizeof(policyObserver),NULL,verifyDialPolicies,NULL};
static xnetaddrlist* dialEndpoints(Graph* graph)
{
    xnetaddr addresses[2];CHECK(xl6ExecutionTcpLocalV1(graph->listener,&addresses[1]));
    CHECK(xrtNetAddrLoopback(&addresses[0],XNET_FAMILY_IPV6,0));
    xnetaddrlist* result=xrtNetAddrListCreate(addresses,2);CHECK(result);return result;
}
static void dialConfig(xnetdialconfig* config)
{xrtNetDialConfigInit(config);config->Family=XNET_FAMILY_IPV4;config->Stream.ReadMode=XNET_STREAM_READ_PROBE;}
static void dialStart(Graph* graph,bool promote)
{
    graph->pending=xl6ExecutionTcpPrepareDialV1(graph->source,graph->engine,NULL);CHECK(graph->pending);
    CHECK(!xl6ExecutionTcpAcceptSourceV1(graph->pending));xrtClearError();
    xnetaddrlist* list=dialEndpoints(graph);xnetdialconfig config;dialConfig(&config);
    /* A real queued command makes Engine BUSY until its Worker consumes it.
     * Freeze before submission makes that boundary deterministic; the test
     * MUST NOT force graph admission while the queue is in this phase. */
    xrtownershipscope frozen={0};xrtownershipsnapshot* proof=snapshot(graph,&frozen);
    xrtOwnershipSnapshotDestroy(proof);
    xfuture* future=xl6ExecutionTcpDialAddressesSourceV1(graph->pending,list,&config);CHECK(future);
    CHECK(xrtFutureState(future)==XFUTURE_PENDING);xrtFutureDestroy(future);
    xrtownershipref anchors[6]={xl6ExecutionSourceOwnershipV1(graph->source),
        xl6ExecutionResourceOwnershipV1(graph->engine),xl6ExecutionResourceOwnershipV1(graph->listener),
        xl6ExecutionResourceOwnershipV1(graph->client),xl6ExecutionResourceOwnershipV1(graph->peer),
        xl6ExecutionResourceOwnershipV1(graph->pending)};
    xrtownershipref owners[5]={anchors[0],anchors[2],anchors[3],anchors[4],anchors[5]};
    graph->busy=false;proof=NULL;
    CHECK(!xrtOwnershipSnapshotCreate(anchors,6,owners,5,admit,graph,&proof)&&graph->busy&&!proof);
    xrtClearError();++dialFrozenProofs;
    future=xrtFutureRef(xrtAtomicPtrLoad(&((XL6_TcpStreamResourceV1*)graph->pending)->pending,XMEMORY_ACQUIRE));CHECK(future);
    CHECK(xrtOwnershipScopeEnd(&frozen));
    CHECK(!xl6ExecutionTcpDialAddressesSourceV1(graph->pending,list,&config));xrtClearError();
    xrtNetAddrListDestroy(list); /* native constructor retained the actual list */
    if(promote){
        CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
        CHECK(!xl6ExecutionTcpPromoteAcceptV1(graph->pending));xrtClearError();
        CHECK(xl6ExecutionTcpPromoteDialV1(graph->pending));
        CHECK(!xl6ExecutionTcpPromoteDialV1(graph->pending));xrtClearError();
        XL6_TcpScopeV1 scope={0};CHECK(xl6ExecutionTcpEnterInternalV1(graph->pending,XL6_EXECUTION_TCP_STREAM_V1,true,&scope));
        CHECK(xrtNetStreamSend(scope.stream,"a\0b",3)==XNET_RESULT_OK);xl6ExecutionTcpLeaveInternalV1(&scope);
        CHECK(xl6ExecutionTcpEnterInternalV1(graph->listener,XL6_EXECUTION_TCP_LISTENER_V1,true,&scope));
        xnetstream* peer=NULL;xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
        while(!(peer=xrtNetListenerAccept(scope.listener))){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
        xl6ExecutionTcpLeaveInternalV1(&scope);
        size_t offset=0;while(offset<3){xfuture* read=xrtNetStreamRecvAsync(peer,3-offset);CHECK(read);
            CHECK(xrtFutureWaitFor(read,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(read)==XFUTURE_RESOLVED);
            xbytesview bytes=xrtNetBytesView(xrtFutureValue(read));
            CHECK(bytes.Size&&bytes.Size<=3-offset&&!memcmp(bytes.Data,"a\0b"+offset,bytes.Size));
            offset+=bytes.Size;xrtFutureDestroy(read);}
        CHECK(xrtNetStreamAbort(peer));xrtNetStreamDestroy(peer);++dialRoundTrips;
    }
    xrtFutureDestroy(future);
}
static void dialConstruction(xnetportbackend backend)
{
    for(unsigned kind=0;kind<2;++kind)for(unsigned budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true);
        xnetaddrlist* list=dialEndpoints(&graph);xnetdialconfig config;dialConfig(&config);xfuture* future=NULL;
        if(kind)graph.pending=xl6ExecutionTcpPrepareDialV1(graph.source,graph.engine,NULL);
        CHECK(!kind||graph.pending);CHECK(xrtMemDebugFailAfter(budget));++dialBudgets;
        if(!kind)graph.pending=xl6ExecutionTcpPrepareDialV1(graph.source,graph.engine,NULL);
        else future=xl6ExecutionTcpDialAddressesSourceV1(graph.pending,list,&config);
        bool failure=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(kind?!future:!graph.pending){CHECK(failure);++dialFailures;}else CHECK(!failure);
        xrtClearError();
        if(kind&&!future){future=xl6ExecutionTcpDialAddressesSourceV1(graph.pending,list,&config);CHECK(future);}
        xrtFutureDestroy(future);xrtNetAddrListDestroy(list);dropOrdinary(&graph);balanced(&before);
    }
}
static void dialAbandon(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true);
    graph.pending=xl6ExecutionTcpPrepareDialV1(graph.source,graph.engine,NULL);CHECK(graph.pending);
    xnetaddrlist* list=dialEndpoints(&graph);xnetdialconfig config;dialConfig(&config);
    xfuture* future=xl6ExecutionTcpDialAddressesSourceV1(graph.pending,list,&config);CHECK(future);xrtNetAddrListDestroy(list);
    CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
    xnetstream* observer=xrtNetStreamRef(xrtFutureValue(future));CHECK(observer);
    xl6ExecutionResourceReleaseV1(graph.pending);graph.pending=NULL;xrtFutureDestroy(future);
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    while(xrtNetStreamState(observer)!=XNET_STREAM_CLOSED){CHECK(!xrtDeadlineExpired(until));xrtThreadYield();}
    xrtNetStreamDestroy(observer);dropOrdinary(&graph);balanced(&before);++dialAbandoned;
}
int main(int argc,char** argv)
{
    CHECK(tcpResourceRegressionMain(argc,argv)==0);CHECK(xrtMemDebugEnable(true));
    const XL6_OwnershipDomainV1* domains[]={
        #include "tcp-resource-domains.h"
        &policyObserver,
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned state=0;state<4;++state)for(unsigned order=0;order<8;++order){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true);
        if(state<2)dialStart(&graph,state!=0);
        else{
            Resource* resolver=state==3?xl6ExecutionNetResolverCreateV1(graph.source,NULL):NULL;CHECK(state!=3||resolver);
            graph.pending=xl6ExecutionTcpPrepareDialV1(graph.source,graph.engine,resolver);CHECK(graph.pending);
            if(resolver){xnetaddrlist* list=dialEndpoints(&graph);
                CHECK(!xl6ExecutionTcpDialAddressesSourceV1(graph.pending,list,NULL));xrtClearError();xrtNetAddrListDestroy(list);
            }else{CHECK(!xl6ExecutionTcpDialSourceV1(graph.pending,"127.0.0.1",1,NULL));xrtClearError();}
            xl6ExecutionResourceReleaseV1(resolver); /* only the actual carrier parent remains */
        }
        collect(&graph,order);balanced(&before);++dialGraphs;
    }
    dialAbandon(backend);dialConstruction(backend);
    CHECK(dialFailures>0&&dialFailures<dialBudgets);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("managed Dial: graphs=%u NUL-roundtrips=%u abandoned=%u budgets=%u failures=%u frozen-proofs=%u; balanced backend=%d\n",
        dialGraphs,dialRoundTrips,dialAbandoned,dialBudgets,dialFailures,dialFrozenProofs,(int)backend);return 0;
}
