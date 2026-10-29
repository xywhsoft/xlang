/* Real native results and resident class publication, ordinary own loopback.
 * No surrogate NetBytes, generated callback or copied payload certificate. */
#define main existingServerResourceMain
#include "release/include/xl_clock.h"
#include "tcp_server_execution_resource.c"
#undef main
#define XL6_TEXT_API
#include "src/runtime/xl6_net_bytes_abi.h"
#include "src/runtime/xl6_tcp_recv_owned_abi.h"
#include "src/runtime/xl6_tcp_recv_owned_internal.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_deadline_module_abi.h"
static const xstrview field={"native",6};
static XL6_NativeClassFieldContextV1 context;
static unsigned received,refused,resultFailures;
static xvalue* emptyObject(void)
{
    void* raw=xl6NetBytesCreateV1();CHECK(raw);
    xvalue* handle=xl6NativeHandleTakeV1(&raw,context.policy);CHECK(handle&&!raw);
    xvalue* object=xrtValueObject();CHECK(object&&xrtValueObjectSet(object,field,handle));
    xrtValueRelease(handle);return object;
}
static void* payload(const xvalue* object)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    CHECK(xrtValueGetHandle(xrtValueObjectGet(object,field),&raw,&ops,&user)&&raw&&!user&&ops==context.policy->Ops);
    return raw;
}
static void idle(XL6_ContinuationExecutor* executor)
{
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xl6ContinuationExecutorIsIdle(executor)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
}
typedef struct WorkerGate {
    XL6_ContinuationTicket ticket;xatomic32 entered,open;unsigned budget;bool clear,triggered;
} WorkerGate;
static void gateRun(void* raw)
{
    WorkerGate* gate=raw;
    if(gate->clear){gate->triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();return;}
    xl6_error_scope warm={0};CHECK(xl6ErrorScopeBegin(&warm));
    xl6_error_result warmed=xl6ErrorScopeEnd(&warm);CHECK(warmed.ok);xrtErrorFree(warmed.error);
    xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtAtomic32Load(&gate->open,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    CHECK(xrtMemDebugFailAfter(gate->budget));
}
static void gateDrop(void* raw){(void)raw;}
static void queueGate(XL6_ContinuationExecutor* executor,WorkerGate* gate)
{
    CHECK(xl6ContinuationExecutorReserve(executor,&gate->ticket,gateRun,gateDrop,gate)==XL6_CONTINUATION_ADMITTED);
    xl6ContinuationCommit(&gate->ticket);xl6ContinuationReady(&gate->ticket);xl6ContinuationTransportReleased(&gate->ticket);
}
static void proof(Graph* graph,xvalue* object,void* bytes)
{
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        xrtownershipscope frozen={0};freeze(&frozen);graph->busy=false;
        xrtownershipref anchors[6]={{0}},slots[5]={{0}};
        anchors[0]=xl6ExecutionSourceOwnershipV1(graph->source);anchors[1]=xl6ExecutionResourceOwnershipV1(graph->engine);
        anchors[2]=xl6ExecutionResourceOwnershipV1(graph->server);anchors[3]=xl6ExecutionResourceOwnershipV1(graph->client);
        anchors[4]=xl6ExecutionResourceOwnershipV1(graph->peer);anchors[5]=xrtValueOwnership(object);
        slots[0]=anchors[0];for(size_t i=1;i<5;++i)slots[i]=anchors[i+1];
        xrtownershipsnapshot* snapshot=NULL;
        if(xrtOwnershipSnapshotCreate(anchors,6,slots,5,admit,graph,&snapshot)){
            xrtownershipref native=xrtNetBytesOwnership(bytes);bool seen=false;
            for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){xrtownershipnode node;
                CHECK(xrtOwnershipSnapshotNode(snapshot,i,&node)&&!node.Reachable);
                if(node.Reference.Data==native.Data&&node.Reference.Ops==native.Ops)seen=true;
            }
            CHECK(seen);xrtOwnershipSnapshotDestroy(snapshot);CHECK(xrtOwnershipScopeEnd(&frozen));return;
        }
        CHECK(graph->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(&frozen)&&!xl6DeadlineExpiredV1(until));xrtThreadYield();
    }
}
static void run(xnetportbackend backend,unsigned budget,bool eof)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.NodeCacheBytes=0;
    Graph graph={0};createWithEngineOptions(&graph,backend,true,&config);
    XL6_RuntimeServicesV1 services={xl6ContinuationExecutorCreateFixedV1(8,1),NULL};CHECK(services.executor);
    xvalue* object=emptyObject();size_t maximum=0;
    xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error));
    CHECK(xrtMemDebugFailAfter(budget));
    XL6_TcpClassCaptureV1* capture=xl6TcpRecvOwnedPrepareInternalV1(context,graph.peer,object,1,&maximum);
    xfuture* observer=NULL;xfuture* result=NULL;
    if(capture){
        xfuture* source=xl6TcpRecvOwnedSourceInternalV1(graph.peer,maximum);
        if(source)observer=xrtFutureRef(source);
        result=xl6TcpClassCapturePublishInternalV1(services,capture,source);
    }
    bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&error);
    if(!result){CHECK(triggered&&!outcome.ok);++refused;}
    else{
        CHECK(outcome.ok&&!triggered&&observer);
        XL6_TcpScopeV1 scope={0};CHECK(xl6ExecutionTcpEnterInternalV1(graph.client,XL6_EXECUTION_TCP_STREAM_V1,true,&scope));
        if(eof)CHECK(xrtNetStreamShutdownWrite(scope.stream));
        else CHECK(xrtNetStreamSend(scope.stream,"\0",1)==XNET_RESULT_OK);
        xl6ExecutionTcpLeaveInternalV1(&scope);
        xfutureresult done={0};CHECK(xrtFutureWaitFor(result,UINT64_C(5000000))==XWAIT_OK&&xrtFutureResult(result,&done));
        CHECK(done.State==XFUTURE_RESOLVED&&done.Value&&*(xvalue**)done.Value==object);
        void* raw=payload(object);xbytesview view=xrtNetBytesView(raw);
        CHECK(view.Size==(eof?0u:1u)&&(!view.Size||view.Data[0]==0));
        CHECK(xrtFutureState(observer)==(eof?XFUTURE_CLOSED:XFUTURE_RESOLVED));
        if(!eof)CHECK(raw==xrtFutureValue(observer)); /* Exact native block, NOT a clone. */
        ++received;
    }
    xrtErrorFree(outcome.error);xrtFutureDestroy(result);xrtFutureDestroy(observer);idle(services.executor);
    if(result)proof(&graph,object,payload(object));
    xl6ContinuationExecutorClose(services.executor);CHECK(xl6ContinuationExecutorDestroy(services.executor));
    dropOrdinary(&graph);
    if(result)CHECK(xl6NetBytesLengthV1(payload(object))==(eof?0:1));
    xrtValueRelease(object);balanced(&before);
}
static void resultBudget(xnetportbackend backend,unsigned budget)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    Graph graph={0};create(&graph,backend,true);
    XL6_RuntimeServicesV1 services={xl6ContinuationExecutorCreateFixedV1(8,1),NULL};CHECK(services.executor);
    xvalue* object=emptyObject();WorkerGate gate={0};gate.budget=budget;
    xrtAtomic32Init(&gate.entered,0);xrtAtomic32Init(&gate.open,0);queueGate(services.executor,&gate);
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtAtomic32Load(&gate.entered,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    size_t maximum=0;XL6_TcpClassCaptureV1* capture=xl6TcpRecvOwnedPrepareInternalV1(context,graph.peer,object,1,&maximum);CHECK(capture);
    xfuture* source=xl6TcpRecvOwnedSourceInternalV1(graph.peer,maximum);CHECK(source);
    xfuture* observer=xrtFutureRef(source);CHECK(observer);
    XL6_TcpScopeV1 scope={0};CHECK(xl6ExecutionTcpEnterInternalV1(graph.client,XL6_EXECUTION_TCP_STREAM_V1,true,&scope));
    CHECK(xrtNetStreamSend(scope.stream,"q",1)==XNET_RESULT_OK);xl6ExecutionTcpLeaveInternalV1(&scope);
    CHECK(xrtFutureWaitFor(observer,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(observer)==XFUTURE_RESOLVED);
    xfuture* result=xl6TcpClassCapturePublishInternalV1(services,capture,source);CHECK(result);
    xrtAtomic32Store(&gate.open,1,XMEMORY_RELEASE);
    xfutureresult done={0};CHECK(xrtFutureWaitFor(result,UINT64_C(5000000))==XWAIT_OK&&xrtFutureResult(result,&done));
    WorkerGate clear={0};clear.clear=true;queueGate(services.executor,&clear);idle(services.executor);
    if(clear.triggered){CHECK(done.State==XFUTURE_FAILED);++resultFailures;}
    else CHECK(done.State==XFUTURE_RESOLVED&&done.Value&&*(xvalue**)done.Value==object&&payload(object)==xrtFutureValue(observer));
    xrtFutureDestroy(result);xrtFutureDestroy(observer);idle(services.executor);
    proof(&graph,object,payload(object));
    xl6ContinuationExecutorClose(services.executor);CHECK(xl6ContinuationExecutorDestroy(services.executor));
    dropOrdinary(&graph);xrtValueRelease(object);balanced(&before);
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope initial={0};CHECK(xl6ErrorScopeBegin(&initial));
    xl6_error_result warm=xl6ErrorScopeEnd(&initial);CHECK(warm.ok);xrtErrorFree(warm.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "tcp-server-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    context=(XL6_NativeClassFieldContextV1){field.Data,field.Size,xl6NetBytesHandlePolicyV1Get()};
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #if defined(_WIN32)||defined(_WIN64)
        backend=XNET_PORT_IOCP;
        #elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned budget=0;budget<24;++budget)run(backend,budget,false);
    run(backend,128,true);CHECK(received&&refused);
    for(unsigned budget=0;budget<5;++budget)resultBudget(backend,budget);
    CHECK(resultFailures>0&&resultFailures<5);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("TCP owned receives: budgets=24 accepted=%u refused=%u result-budgets=5 failures=%u, exact native pointer, complete graph, EOF and independent payload lifetime; balanced backend=%d\n",received,refused,resultFailures,(int)backend);return 0;
}
