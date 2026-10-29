/* Reuse the real native implementation/catalog and ordinary graph helpers.
 * No production seam, native replacement, public network or protocol test. */
#define main existingServerResourceMain
#include "release/include/xl_clock.h"
#include "tcp_server_execution_resource.c"
#undef main
#include "src/runtime/xl6_execution_language.h"
extern void* xl6TcpServerTryAcceptV1(XL6_ExecutionServicesV1,void*);

static Resource* pollServer(Graph* graph,bool* ok)
{
    xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
    Resource* result=xl6TcpServerTryAcceptV1((XL6_ExecutionServicesV1){graph->source},graph->server);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);*ok=outcome.ok;
    xrtErrorFree(outcome.error);return result;
}
static void testPoll(xnetportbackend backend,unsigned budget,unsigned* refusedCount)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    Graph graph={0};create(&graph,backend,true);bool ok=false;
    CHECK(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<1000;++i)CHECK(!pollServer(&graph,&ok)&&ok&&!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xnetaddr address;CHECK(serverLocal(graph.server,&address));
    xnetstreamconfig config;xrtNetStreamConfigInit(&config);config.ReadMode=XNET_STREAM_READ_PROBE;
    Resource* client=xl6ExecutionTcpConnectV1(graph.source,graph.engine,&address,0,&config);CHECK(client);
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        XL6_TcpServerScopeV1 scope={0};xnetserverstats stats={0};
        CHECK(xl6ExecutionTcpServerEnterInternalV1(graph.server,false,&scope));
        CHECK(xrtNetServerStats(scope.server,&stats));xl6ExecutionTcpServerLeaveInternalV1(&scope);
        if(stats.QueuedAccepts==1)break;
        CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();
    }
    CHECK(xrtMemDebugFailAfter(budget));
    Resource* peer=pollServer(&graph,&ok);bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(!peer){CHECK(!ok&&triggered);++*refusedCount;}else CHECK(ok&&!triggered);
    CHECK(!pollServer(&graph,&ok)&&ok);
    xl6ExecutionResourceReleaseV1(peer);xl6ExecutionResourceReleaseV1(client);
    CHECK(xl6ExecutionResourceCloseV1(graph.server));
    CHECK(!pollServer(&graph,&ok)&&!ok);
    dropOrdinary(&graph);balanced(&before);
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));
    /* Initialize error TLS before allocation injection. */
    xl6_error_scope initial={0};CHECK(xl6ErrorScopeBegin(&initial));
    xl6_error_result error=xl6ErrorScopeEnd(&initial);CHECK(error.ok);xrtErrorFree(error.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "tcp-server-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #if defined(_WIN32) || defined(_WIN64)
        backend=XNET_PORT_IOCP;
        #elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
        #endif
    }
    unsigned refusedCount=0;
    for(unsigned budget=0;budget<16;++budget)testPoll(backend,budget,&refusedCount);
    CHECK(refusedCount>0&&refusedCount<16);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("Server owning poll: empty=16000 allocation-free, budgets=16 failures=%u; checked-close, abort rollback and balanced backend=%d\n",
        refusedCount,(int)backend);return 0;
}
