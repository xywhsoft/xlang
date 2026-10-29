#define main existingServerResourceMain
#include "tcp_server_execution_resource.c"
#undef main
#define XL6_TEXT_API
#include "src/runtime/xl6_net_bytes_abi.h"
#include "src/runtime/xl6_native_handle.h"
/* Sequence's unrelated Set-policy pointer is opaque in this pruned closure. */
typedef struct xtypedsetkeypolicy xtypedsetkeypolicy;
#include "src/runtime/xl6_array_admission.h"
static xvalue* bytesArray(void** payload,size_t count)
{
    void* raw=xl6NetBytesFromTextV1((xstrview){"a\0bc",4},-1);CHECK(raw);*payload=raw;
    xvalue* handle=xl6NativeHandleTakeV1(&raw,xl6NetBytesHandlePolicyV1Get());CHECK(handle&&!raw);
    xvalue* object=xrtValueObject();xvalue* tag=xrtValueInt(101);CHECK(object&&tag);
    CHECK(xrtValueObjectSet(object,(xstrview){"$xlir.class.runtime-type",24},tag)&&
        xrtValueObjectSet(object,(xstrview){"handle",6},handle));xrtValueRelease(tag);xrtValueRelease(handle);
    uint64_t classTag=101;XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,0,&classTag,1};
    xvalue* array=xl6SequenceNominalCreateV1(1,201,202,&schema,false,202);CHECK(array);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(array,1,201,202,3,sizeof(xvalue*),xrtTypeValue());CHECK(pin&&xl6SequenceBindProjectionV1(pin,NULL));
    for(size_t i=0;i<count;++i)CHECK(xl6SequenceAdmitSlotV1(pin,&object)&&xrtTypedArrayPush(xl6SequenceStorageV1(pin),&object));
    xl6SequenceEndV1(pin);xrtValueRelease(object);return array;
}
static void proof(Graph* graph,xvalue* array,void* bytes)
{
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){
        xrtownershipscope frozen={0};freeze(&frozen);graph->busy=false;
        xrtownershipref anchors[6]={{0}},slots[5]={{0}};
        anchors[0]=xl6ExecutionSourceOwnershipV1(graph->source);anchors[1]=xl6ExecutionResourceOwnershipV1(graph->engine);
        anchors[2]=xl6ExecutionResourceOwnershipV1(graph->server);anchors[3]=xl6ExecutionResourceOwnershipV1(graph->client);
        anchors[4]=xl6ExecutionResourceOwnershipV1(graph->peer);anchors[5]=xrtValueOwnership(array);
        slots[0]=anchors[0];for(size_t i=1;i<5;++i)slots[i]=anchors[i+1];xrtownershipsnapshot* snapshot=NULL;
        if(xrtOwnershipSnapshotCreate(anchors,6,slots,5,admit,graph,&snapshot)){
            xrtownershipref native=xrtNetBytesOwnership(bytes);bool seen=false;
            for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){xrtownershipnode node;
                CHECK(xrtOwnershipSnapshotNode(snapshot,i,&node)&&!node.Reachable);
                if(node.Reference.Data==native.Data&&node.Reference.Ops==native.Ops)seen=true;
            }
            CHECK(seen);xrtOwnershipSnapshotDestroy(snapshot);CHECK(xrtOwnershipScopeEnd(&frozen));return;
        }
        CHECK(graph->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(&frozen)&&!xrtDeadlineExpired(until));xrtThreadYield();
    }
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope warm={0};CHECK(xl6ErrorScopeBegin(&warm));
    xl6_error_result initial=xl6ErrorScopeEnd(&warm);CHECK(initial.ok);xrtErrorFree(initial.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "tcp-server-resource-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    unsigned accepted=0,refused=0;
    XL6_NativeClassArrayContextV1 context={{201,202,3,sizeof(xvalue*),xrtTypeValue()},"handle",6,xl6NetBytesHandlePolicyV1Get()};
    for(unsigned trial=0;trial<20;++trial){
        unsigned budget=trial<16?trial:trial-16;size_t count=trial<16?2:65;
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        xnetengineconfig config;xrtNetEngineConfigInit(&config);config.NodeCacheBytes=0;
        Graph graph={0};createWithEngineOptions(&graph,backend,true,&config);
        void* raw=NULL;xvalue* array=bytesArray(&raw,count);xl6_error_scope error={0};CHECK(xl6ErrorScopeBegin(&error));
        CHECK(xrtMemDebugFailAfter(budget));int64_t result=xl6TcpStreamSendRefsV1(context,graph.client,array,6);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result outcome=xl6ErrorScopeEnd(&error);
        if(result==XNET_RESULT_ERROR){CHECK(triggered&&!outcome.ok);++refused;}
        else{CHECK(result==XNET_RESULT_OK&&!triggered&&outcome.ok);++accepted;}
        xrtErrorFree(outcome.error);proof(&graph,array,raw);xrtValueRelease(array);
        if(result==XNET_RESULT_OK){size_t offset=0;
            while(offset<6){XL6_TcpScopeV1 scope={0};CHECK(xl6ExecutionTcpEnterInternalV1(graph.peer,XL6_EXECUTION_TCP_STREAM_V1,true,&scope));
                xfuture* input=xrtNetStreamRecvAsync(scope.stream,6-offset);CHECK(input);xl6ExecutionTcpLeaveInternalV1(&scope);
                CHECK(xrtFutureWaitFor(input,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(input)==XFUTURE_RESOLVED);
                xbytesview view=xrtNetBytesView(xrtFutureValue(input));CHECK(view.Size&&view.Size<=6-offset&&!memcmp(view.Data,"a\0bca\0"+offset,view.Size));
                offset+=view.Size;xrtFutureDestroy(input);
            }
        }
        dropOrdinary(&graph);balanced(&before);
    }
    CHECK(accepted&&refused);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("TCP owned ref batches: budgets=16 large-budgets=4 accepted=%u refused=%u, exact class-array graph, duplicate byte owners, source lifetime and NUL; balanced backend=%d\n",accepted,refused,(int)backend);return 0;
}
