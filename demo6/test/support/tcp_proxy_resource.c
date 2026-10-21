/* Real carrier/Resolver/Proxy ownership and constructor prefixes. This also
 * runs the original TCP graph suite; no synthetic graph capability is added. */
#define main tcpResourceRegressionMain
#include "tcp_execution_resource.c"
#undef main
#include "src/runtime/xl6_execution_tcp_dial.h"
#include "src/runtime/xl6_execution_net_resolver.h"
static unsigned proxyGraphs,proxyBudgets,proxyFailures;
static bool verifyPolicies(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** finish)
{
    (void)ref;(void)adapter;(void)preparation;(void)finish;
    bool root=false,inner=false,done=false,events=false,producer=false;
    for(size_t i=0;i<policies->NetRuntimeCount;++i){
        root|=policies->NetRuntime[i]==xrtNetProxyDialRuntimeOwnershipV1Get();
        inner|=policies->NetRuntime[i]==xrtNetDialRuntimeOwnershipV1Get();}
    for(size_t i=0;i<policies->ProxyDialDoneCount;++i)done|=policies->ProxyDialDone[i]==xrtNetProxyDialFutureDialPolicyV1Get();
    for(size_t i=0;i<policies->StreamEventsCount;++i)events|=policies->StreamEvents[i]==xrtNetProxyDialStreamOwnershipV1Get();
    for(size_t i=0;i<policies->ProducerCount;++i)producer|=policies->Producer[i]==xrtNetProxyDialFutureProducerPolicyV1Get();
    CHECK(root&&inner&&done&&events&&producer);return false;
}
static const XL6_OwnershipDomainV1 observer={sizeof(observer),NULL,verifyPolicies,NULL};
static xnetproxy* endpoint(Graph* graph)
{
    xnetaddr address;CHECK(xl6ExecutionTcpLocalV1(graph->listener,&address));
    xnetproxyconfig config;xrtNetProxyConfigInit(&config);config.Type=XNET_PROXY_SOCKS5;
    config.Host=(xstrview){"127.0.0.1",9};config.Port=address.Port;
    xnetproxy* proxy=xrtNetProxyCreate(&config);CHECK(proxy);return proxy;
}
static Resource* resolver(Graph* graph)
{Resource* value=xl6ExecutionNetResolverCreateV1(graph->source,NULL);CHECK(value);return value;}
static xfuture* submit(Graph* graph,xnetproxy* proxy)
{
    xnetproxydialconfig config;xrtNetProxyDialConfigInit(&config);
    config.Transport.Family=XNET_FAMILY_IPV4;
    return xl6ExecutionTcpProxyDialSourceV1(graph->pending,proxy,"origin.test",8443,&config);
}
int main(int argc,char** argv)
{
    CHECK(tcpResourceRegressionMain(argc,argv)==0);CHECK(xrtMemDebugEnable(true));
    const XL6_OwnershipDomainV1* domains[]={
        #include "tcp-resource-domains.h"
        &observer,
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned started=0;started<2;++started)for(unsigned order=0;order<8;++order){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true);
        Resource* parent=resolver(&graph);graph.pending=xl6ExecutionTcpPrepareDialV1(graph.source,graph.engine,parent);CHECK(graph.pending);
        xl6ExecutionResourceReleaseV1(parent); /* only the actual carrier parent remains */
        if(started){xnetproxy* proxy=endpoint(&graph);xfuture* future=submit(&graph,proxy);CHECK(future);
            CHECK(!submit(&graph,proxy));xrtClearError();xrtFutureDestroy(future);xrtNetProxyRelease(proxy);}
        collect(&graph,order);balanced(&before);++proxyGraphs;
    }
    for(unsigned kind=0;kind<2;++kind)for(unsigned budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true);
        Resource* parent=resolver(&graph);xnetproxy* proxy=endpoint(&graph);xfuture* future=NULL;
        if(kind){graph.pending=xl6ExecutionTcpPrepareDialV1(graph.source,graph.engine,parent);CHECK(graph.pending);}
        CHECK(xrtMemDebugFailAfter(budget));++proxyBudgets;
        if(kind)future=submit(&graph,proxy);else graph.pending=xl6ExecutionTcpPrepareDialV1(graph.source,graph.engine,parent);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(kind?!future:!graph.pending){CHECK(triggered);++proxyFailures;}else CHECK(!triggered);
        xrtClearError();xrtFutureDestroy(future);xrtNetProxyRelease(proxy);xl6ExecutionResourceReleaseV1(parent);
        dropOrdinary(&graph);balanced(&before);
    }
    CHECK(proxyFailures>0&&proxyFailures<proxyBudgets);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("managed Proxy: graphs=%u budgets=%u failures=%u; exact protocols, real Resolver parent, balanced backend=%d\n",
        proxyGraphs,proxyBudgets,proxyFailures,(int)backend);return 0;
}
