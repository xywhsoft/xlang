/* Real source/native resources and their current graph; normal loopback only. */
#define main existingServerResourceMain
#include "tcp_server_execution_resource.c"
#include "release/include/xl_clock.h"
#undef main
#define XL6_TEXT_API
#include "src/runtime/xl6_net_bytes_abi.h"
#include "src/runtime/xl6_native_handle.h"

static int64_t sendOwned(Graph* graph,void* input,bool moving,bool* ok)
{
    xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error));
    int64_t result=moving?xl6TcpStreamSendMoveV1(graph->client,input):
        xl6TcpStreamSendRefV1(graph->client,input,20,1);
    xl6_error_result outcome=xl6ErrorScopeEnd(&error);*ok=outcome.ok;xrtErrorFree(outcome.error);return result;
}
static void readExact(Graph* graph,const uint8* expected,size_t count)
{
    size_t received=0;
    while(received<count){
        XL6_TcpScopeV1 scope={0};CHECK(xl6ExecutionTcpEnterInternalV1(graph->peer,XL6_EXECUTION_TCP_STREAM_V1,true,&scope));
        xfuture* future=xrtNetStreamRecvAsync(scope.stream,count-received);CHECK(future);xl6ExecutionTcpLeaveInternalV1(&scope);
        CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
        xbytesview view=xrtNetBytesView(xrtFutureValue(future));
        CHECK(view.Size&&view.Size<=count-received&&!memcmp(view.Data,expected+received,view.Size));
        received+=view.Size;xrtFutureDestroy(future);
    }
}
static void completeGraph(Graph* graph,xvalue* payload)
{
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        xrtownershipscope frozen={0};freeze(&frozen);graph->busy=false;
        xrtownershipref anchors[6]={{0}},slots[5]={{0}};
        anchors[0]=xl6ExecutionSourceOwnershipV1(graph->source);anchors[1]=xl6ExecutionResourceOwnershipV1(graph->engine);
        anchors[2]=xl6ExecutionResourceOwnershipV1(graph->server);anchors[3]=xl6ExecutionResourceOwnershipV1(graph->client);
        anchors[4]=xl6ExecutionResourceOwnershipV1(graph->peer);anchors[5]=xrtValueOwnership(payload);
        slots[0]=anchors[0];for(size_t i=1;i<5;++i)slots[i]=anchors[i+1];
        xrtownershipsnapshot* proof=NULL;
        if(xrtOwnershipSnapshotCreate(anchors,6,slots,5,admit,graph,&proof)){
            bool seen=false;
            for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode node;
                CHECK(xrtOwnershipSnapshotNode(proof,i,&node)&&!node.Reachable);
                if(node.Reference.Data==anchors[5].Data&&node.Reference.Ops==anchors[5].Ops)seen=true;
            }
            CHECK(seen);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&frozen));return;
        }
        CHECK(graph->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(&frozen)&&!xl6DeadlineExpiredV1(until));xrtThreadYield();
    }
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope initial={0};CHECK(xl6ErrorScopeBegin(&initial));
    xl6_error_result warm=xl6ErrorScopeEnd(&initial);CHECK(warm.ok);xrtErrorFree(warm.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "tcp-server-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #if defined(_WIN32)||defined(_WIN64)
        backend=XNET_PORT_IOCP;
        #elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
        #endif
    }
    unsigned refusedCount=0,acceptedCount=0;
    const uint8 data[]={'a',0,'b',255};
    for(unsigned moving=0;moving<2;++moving)for(unsigned budget=0;budget<8;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        /* Disable the documented node cache to make EACH injection point
         * exercise the allocator rather than a prewarmed cached send node. */
        xnetengineconfig config;xrtNetEngineConfigInit(&config);config.NodeCacheBytes=0;
        Graph graph={0};createWithEngineOptions(&graph,backend,true,&config);
        void* input=moving?(void*)xrtBufferFrom((xbytesview){data,4}):xl6NetBytesFromTextV1((xstrview){(const char*)data,4},-1);CHECK(input);
        void* raw=input;
        xvalue* handle=xl6NativeHandleTakeV1(&raw,moving?xl6BufferHandlePolicyV1Get():xl6NetBytesHandlePolicyV1Get());CHECK(handle&&!raw);
        uint8* original=moving?((xbuffer*)input)->Data:NULL;size_t capacity=moving?((xbuffer*)input)->Capacity:0;
        CHECK(xrtMemDebugFailAfter(budget));bool ok=false;
        int64_t result=sendOwned(&graph,input,moving!=0,&ok);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(result==XNET_RESULT_ERROR){
            CHECK(triggered&&!ok);++refusedCount;
            if(moving)CHECK(((xbuffer*)input)->Data==original&&((xbuffer*)input)->Size==4&&
                ((xbuffer*)input)->Capacity==capacity&&!memcmp(original,data,4));
            else{
                size_t references=0;xrtownershipscope frozen={0};freeze(&frozen);
                xrtownershipref payload=xrtNetBytesOwnership(input);
                CHECK(payload.Ops->Count(payload.Data,&references)&&references==1);
                CHECK(xrtOwnershipScopeEnd(&frozen));
            }
        }else{
            CHECK(result==XNET_RESULT_OK&&ok&&!triggered);++acceptedCount;
            if(moving)CHECK(!((xbuffer*)input)->Data&&!((xbuffer*)input)->Size&&!((xbuffer*)input)->Capacity);
            completeGraph(&graph,handle);
            /* User owner disappears before receiving. Only actual native
             * queued/completion ownership (if still pending) may retain it. */
            xrtValueRelease(handle);handle=NULL;
            readExact(&graph,moving?data:data+1,moving?4:3);
        }
        xrtValueRelease(handle);
        CHECK(xl6ExecutionResourceCloseV1(graph.client));
        xbuffer* unchanged=xrtBufferFrom((xbytesview){data,4});CHECK(unchanged);
        original=unchanged->Data;capacity=unchanged->Capacity;
        CHECK(sendOwned(&graph,unchanged,true,&ok)==XNET_RESULT_ERROR&&!ok);
        CHECK(unchanged->Data==original&&unchanged->Size==4&&unchanged->Capacity==capacity);xrtBufferDestroy(unchanged);
        dropOrdinary(&graph);balanced(&before);
    }
    CHECK(refusedCount&&acceptedCount);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("TCP owned sends: budgets=16 accepted=%u rejected=%u, exact whole owning graph, NUL retained slice/Buffer take, source unchanged on OOM/close, balanced backend=%d\n",
        acceptedCount,refusedCount,(int)backend);return 0;
}
