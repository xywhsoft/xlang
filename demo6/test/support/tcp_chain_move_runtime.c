#define main existingServerResourceMain
#include "tcp_server_execution_resource.c"
#undef main
#define XL6_TEXT_API
#include "src/runtime/xl6_net_bytes_abi.h"
#include "src/runtime/xl6_net_buffer_abi.h"
#include "src/runtime/xl6_tcp_data_abi.h"
#include "src/runtime/xl6_native_handle.h"
static void graphProof(Graph* graph,xvalue* buffer,xvalue* input,void* bytes)
{
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){
        xrtownershipscope frozen={0};freeze(&frozen);graph->busy=false;
        xrtownershipref anchors[7]={{0}},slots[6]={{0}};
        anchors[0]=xl6ExecutionSourceOwnershipV1(graph->source);anchors[1]=xl6ExecutionResourceOwnershipV1(graph->engine);
        anchors[2]=xl6ExecutionResourceOwnershipV1(graph->server);anchors[3]=xl6ExecutionResourceOwnershipV1(graph->client);
        anchors[4]=xl6ExecutionResourceOwnershipV1(graph->peer);anchors[5]=xrtValueOwnership(buffer);anchors[6]=xrtValueOwnership(input);
        slots[0]=anchors[0];for(size_t i=1;i<6;++i)slots[i]=anchors[i+1];xrtownershipsnapshot* proof=NULL;
        if(xrtOwnershipSnapshotCreate(anchors,7,slots,6,admit,graph,&proof)){
            bool found=false;xrtownershipref expected=xrtNetBytesOwnership(bytes);
            for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode node;
                CHECK(xrtOwnershipSnapshotNode(proof,i,&node)&&!node.Reachable);
                if(node.Reference.Data==expected.Data&&node.Reference.Ops==expected.Ops)found=true;
            }
            CHECK(found);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&frozen));return;
        }
        CHECK(graph->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(&frozen)&&!xrtDeadlineExpired(until));xrtThreadYield();
    }
}
static void checkBytes(void* buffer,const void* expected,size_t size)
{
    xvalue* copy=xl6NetBufferPeekV1(buffer,0,-1);xbytesview view={0};
    CHECK(copy&&xrtValueGetBytes(copy,&view)&&view.Size==size&&(!size||!memcmp(view.Data,expected,size)));xrtValueRelease(copy);
}
static void appendBudgets(void)
{
    unsigned failures=0;
    for(unsigned budget=0;budget<4;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        void* bytes=xl6NetBytesFromTextV1((xstrview){"XbcY",4},-1);void* buffer=xl6NetBufferCreateV1(8);CHECK(bytes&&buffer);
        xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error)&&xrtMemDebugFailAfter(budget));
        xl6NetBufferAppendRefV1(buffer,bytes,2,1);bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&error);
        if(triggered){CHECK(!outcome.ok&&!xl6NetBufferLengthV1(buffer)&&!xl6NetBufferSpanCountV1(buffer));++failures;}
        else{CHECK(outcome.ok);checkBytes(buffer,"bc",2);}
        xrtErrorFree(outcome.error);xl6NetBufferClearV1(buffer);xl6NetBufferAppendTextV1(buffer,(xstrview){"ok",2},-1);checkBytes(buffer,"ok",2);
        xl6NetBufferDestroyV1(buffer);xl6NetBytesDestroyV1(bytes);balanced(&before);
    }
    CHECK(failures==1);
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope warm={0};CHECK(xl6ErrorScopeBegin(&warm));
    xl6_error_result initial=xl6ErrorScopeEnd(&warm);CHECK(initial.ok);xrtErrorFree(initial.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "tcp-server-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    appendBudgets();
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    unsigned accepted=0,refused=0;
    for(unsigned budget=0;budget<12;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        xnetengineconfig config;xrtNetEngineConfigInit(&config);config.NodeCacheBytes=0;
        Graph graph={0};createWithEngineOptions(&graph,backend,true,&config);
        void* bytes=xl6NetBytesFromTextV1((xstrview){"XbcY",4},-1);CHECK(bytes);void* raw=bytes;
        xvalue* input=xl6NativeHandleTakeV1(&raw,xl6NetBytesHandlePolicyV1Get());CHECK(input&&!raw);
        void* buffer=xl6NetBufferCreateV1(64);CHECK(buffer);raw=buffer;
        xvalue* owner=xl6NativeHandleTakeV1(&raw,xl6NetBufferHandlePolicyV1Get());CHECK(owner&&!raw);
        xl6NetBufferAppendTextV1(buffer,(xstrview){"Xa\0",3},-1);CHECK(xl6NetBufferConsumeV1(buffer,1)==1);
        xl6NetBufferAppendRefV1(buffer,bytes,2,1);CHECK(xl6NetBufferLengthV1(buffer)==4&&xl6NetBufferSpanCountV1(buffer)==2);
        graphProof(&graph,owner,input,bytes);xrtValueRelease(input);input=NULL;
        xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error)&&xrtMemDebugFailAfter(budget));
        int64_t result=xl6TcpStreamSendChainMoveV1(graph.client,buffer);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result outcome=xl6ErrorScopeEnd(&error);
        if(result==XNET_RESULT_ERROR){CHECK(triggered&&!outcome.ok);++refused;checkBytes(buffer,"a\0bc",4);}
        else{
            CHECK(result==XNET_RESULT_OK&&!triggered&&outcome.ok&&!xl6NetBufferLengthV1(buffer)&&!xl6NetBufferSpanCountV1(buffer));++accepted;
            xl6NetBufferAppendTextV1(buffer,(xstrview){"reuse",5},-1);checkBytes(buffer,"reuse",5);
        }
        xrtErrorFree(outcome.error);xrtValueRelease(owner);
        if(result==XNET_RESULT_OK){size_t offset=0;
            while(offset<4){XL6_TcpScopeV1 scope={0};CHECK(xl6ExecutionTcpEnterInternalV1(graph.peer,XL6_EXECUTION_TCP_STREAM_V1,true,&scope));
                xfuture* read=xrtNetStreamRecvAsync(scope.stream,4-offset);CHECK(read);xl6ExecutionTcpLeaveInternalV1(&scope);
                CHECK(xrtFutureWaitFor(read,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(read)==XFUTURE_RESOLVED);
                xbytesview view=xrtNetBytesView(xrtFutureValue(read));CHECK(view.Size&&view.Size<=4-offset&&!memcmp(view.Data,"a\0bc"+offset,view.Size));
                offset+=view.Size;xrtFutureDestroy(read);
            }
        }
        dropOrdinary(&graph);balanced(&before);
    }
    CHECK(accepted&&refused);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("TCP managed chain take: budgets=12 append-budgets=4 append-failures=1 accepted=%u refused=%u, real buffer/bytes graph, rollback, early wrapper drop and reuse, NUL; balanced backend=%d\n",accepted,refused,(int)backend);return 0;
}
