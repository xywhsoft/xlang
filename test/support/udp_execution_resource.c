/* Actual selected resident/native domains. Ordinary literal loopback only.
 * No DNS, public endpoints, malformed traffic or error-queue probes. */
#include "udp-modules.h"
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
#include "src/runtime/xl6_execution_udp_internal.h"
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
typedef struct Graph {XL6_ExecutionSourceV1* source;Resource *engine,*sender,*receiver;xnetaddr address;
    Node nodes[CAPACITY];size_t count;bool pinned,busy;
    #ifdef UDP_FUTURE_ACCEPTANCE
    xfuture* pending;
    #endif
} Graph;
static XL6_OwnershipCatalog* catalog;
static unsigned graphs,ordinary,budgets,failures;
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
        xrtownershipref anchors[CAPACITY+5]={xl6ExecutionSourceOwnershipV1(graph->source),xl6ExecutionResourceOwnershipV1(graph->engine),
            xl6ExecutionResourceOwnershipV1(graph->sender),xl6ExecutionResourceOwnershipV1(graph->receiver)};
        /* No original Engine owner remains; the two real child parent slots
         * own it. Pins are excluded slots, never fictitious owning edges. */
        xrtownershipref slots[CAPACITY+4]={anchors[0],anchors[2],anchors[3]};size_t a=4,s=3;
        #ifdef UDP_FUTURE_ACCEPTANCE
        if(graph->pending){anchors[a++]=xrtFutureOwnership(graph->pending);slots[s++]=anchors[a-1];}
        #endif
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
static void waitOpen(Resource* resource)
{
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);int state=-1;
    for(;;){CHECK(xl6ExecutionUdpStateV1(resource,&state));if(state==XNET_UDP_OPEN)return;
        CHECK(state!=XNET_UDP_CLOSED&&!xl6DeadlineExpiredV1(until));xrtThreadYield();}
}
static void create(Graph* graph,xnetportbackend backend)
{
    graph->source=xl6ExecutionSourceCreateV1();CHECK(graph->source);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;
    graph->engine=xl6ExecutionNetEngineCreateV1(graph->source,&config);CHECK(graph->engine&&xl6ExecutionNetEngineStartV1(graph->engine));
    CHECK(xrtNetAddrLoopback(&graph->address,XNET_FAMILY_IPV4,0));
    graph->receiver=xl6ExecutionUdpBindV1(graph->source,graph->engine,&graph->address,0,NULL);CHECK(graph->receiver);waitOpen(graph->receiver);
    CHECK(xl6ExecutionUdpLocalV1(graph->receiver,&graph->address));
    graph->sender=xl6ExecutionUdpConnectV1(graph->source,graph->engine,&graph->address,0,NULL);CHECK(graph->sender);waitOpen(graph->sender);
    bool connected=false;CHECK(xl6ExecutionUdpConnectedV1(graph->sender,&connected)&&connected);
    xnetaddr peer;CHECK(xl6ExecutionUdpPeerV1(graph->sender,&peer));
    XL6_ExecutionResourceSnapshotV1 state={.size=sizeof(state)};
    CHECK(xl6ExecutionResourceSnapshotV1Get(graph->engine,&state)&&state.owners==3);
    xl6ExecutionResourceReleaseV1(graph->engine);
    CHECK(xl6ExecutionResourceSnapshotV1Get(graph->engine,&state)&&state.owners==2&&!state.closing);
}
static xnetudppacket* receive(Graph* graph,const void* data,size_t size)
{
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);xnetudppacket* packet=NULL;
    while(!(packet=xl6ExecutionUdpReceiveV1(graph->receiver))){CHECK(!xrtGetError()&&!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    CHECK(xrtNetUdpPacketSize(packet)==size&&(!size||!memcmp(xrtNetUdpPacketData(packet),data,size)));
    CHECK(!xrtNetUdpPacketTruncated(packet));return packet;
}
static void roundTrip(Graph* graph)
{
    CHECK(xl6ExecutionUdpSendV1(graph->sender,NULL,NULL,"a\0b",3)==XNET_RESULT_OK);
    xnetudppacket* packet=receive(graph,"a\0b",3);
    CHECK(xl6ExecutionUdpSendPacketV1(graph->sender,NULL,NULL,packet,0,3)==XNET_RESULT_OK);
    xrtNetUdpPacketDestroy(packet);xrtNetUdpPacketDestroy(receive(graph,"a\0b",3));
    static const unsigned char left[]={'a','b'},right[]={0,'c'};
    const xnetspan spans[]={{left,2},{right,2}};
    CHECK(xl6ExecutionUdpSendVecV1(graph->sender,NULL,spans,2)==XNET_RESULT_OK);xrtNetUdpPacketDestroy(receive(graph,"ab\0c",4));
    void* taken=xrtMalloc(3);CHECK(taken);memcpy(taken,"t\0k",3);
    CHECK(xl6ExecutionUdpSendTakeV1(graph->sender,NULL,NULL,taken,3)==XNET_RESULT_OK);xrtNetUdpPacketDestroy(receive(graph,"t\0k",3));
    xnetbytes* bytes=xrtNetBytesCopy("_x\0y_",5);CHECK(bytes);
    CHECK(xl6ExecutionUdpSendBytesV1(graph->sender,NULL,NULL,bytes,1,3)==XNET_RESULT_OK);xrtNetUdpPacketDestroy(receive(graph,"x\0y",3));
    xnetbytesref refs[]={{bytes,1,3},{bytes,0,1}};
    CHECK(xl6ExecutionUdpSendBytesVecV1(graph->sender,NULL,NULL,refs,2)==XNET_RESULT_OK);xrtNetUdpPacketDestroy(receive(graph,"x\0y_",4));
    xnetudpownedmessage items[2]={0};items[0].Kind=XNET_UDP_OWNED_BYTES;items[0].Payload.Bytes=refs[0];
    items[1].Kind=XNET_UDP_OWNED_VECTOR;items[1].Payload.Vector.Items=refs;items[1].Payload.Vector.Count=2;
    size_t accepted=99;CHECK(xl6ExecutionUdpSendOwnedBatchV1(graph->sender,items,2,&accepted)==XNET_RESULT_OK&&accepted==2);
    xrtNetBytesDestroy(bytes);xrtNetUdpPacketDestroy(receive(graph,"x\0y",3));xrtNetUdpPacketDestroy(receive(graph,"x\0y_",4));
    const xnetdgramsend copies[]={{NULL,"b\0c",3},{NULL,"xy",2}};
    CHECK(xl6ExecutionUdpSendBatchV1(graph->sender,copies,2,&accepted)==XNET_RESULT_OK&&accepted==2);
    xrtNetUdpPacketDestroy(receive(graph,"b\0c",3));xrtNetUdpPacketDestroy(receive(graph,"xy",2));
    CHECK(xl6ExecutionUdpSendV1(graph->sender,NULL,NULL,NULL,0)==XNET_RESULT_OK);xrtNetUdpPacketDestroy(receive(graph,NULL,0));
    XL6_UdpScopeV1 scope={0};CHECK(!xl6ExecutionUdpEnterInternalV1(graph->engine,false,&scope)&&xrtErrorKind(xrtGetError())==XERR_TYPE);
    CHECK(!scope.udp&&!scope.execution.resource);xrtClearError();
    xnetudpstats stats;CHECK(xl6ExecutionUdpStatsV1(graph->sender,&stats));uint32_t flags=0;
    CHECK(xl6ExecutionUdpSendControlAvailableV1(graph->sender,&flags));
}
static void dropOrdinary(Graph* graph)
{
    xl6ExecutionResourceReleaseV1(graph->sender);xl6ExecutionResourceReleaseV1(graph->receiver);
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
    #ifdef UDP_FUTURE_ACCEPTANCE
    xrtFutureDestroy(graph->pending);graph->pending=NULL;
    #endif
    xl6ExecutionResourceReleaseV1(graph->sender);xl6ExecutionResourceReleaseV1(graph->receiver);
    XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&graph->source);CHECK(result.ready&&result.ok&&!graph->source);
    for(size_t step=0;step<graph->count;++step){size_t i=order&4?graph->count-1-step:step;graph->nodes[i].adapter->Drop(graph->nodes[i].ref.Data);}
}
static void construction(xnetportbackend backend)
{
    unsigned kinds=2;
    #ifdef UDP_FUTURE_ACCEPTANCE
    kinds=4;
    #endif
    for(unsigned kind=0;kind<kinds;++kind)for(unsigned budget=0;budget<12;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend);
        xnetaddr local;CHECK(xrtNetAddrLoopback(&local,XNET_FAMILY_IPV4,0));
        Resource* result=NULL;bool ok=false;
        #ifdef UDP_FUTURE_ACCEPTANCE
        xfuture* future=NULL;
        #endif
        CHECK(xrtMemDebugFailAfter(budget));++budgets;
        if(kind<2){result=kind?xl6ExecutionUdpConnectV1(graph.source,graph.engine,&graph.address,0,NULL):
            xl6ExecutionUdpBindV1(graph.source,graph.engine,&local,0,NULL);ok=result!=NULL;}
        #ifdef UDP_FUTURE_ACCEPTANCE
        else{future=kind==2?xl6ExecutionUdpReceiveAsyncV1(graph.receiver):xl6ExecutionUdpReceiveBatchAsyncV1(graph.receiver,4);ok=future!=NULL;}
        #endif
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!ok){CHECK(failed);++failures;}else CHECK(!failed);
        #ifdef UDP_FUTURE_ACCEPTANCE
        xrtFutureDestroy(future);
        #endif
        xrtClearError();xl6ExecutionResourceReleaseV1(result);dropOrdinary(&graph);balanced(&before);
    }
}
static void lifecycle(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend);
    CHECK(xl6ExecutionUdpSendV1(graph.sender,NULL,NULL,"p\0q",3)==XNET_RESULT_OK);
    xnetudppacket* survivor=receive(&graph,"p\0q",3);
    CHECK(xl6ExecutionResourceCloseV1(graph.sender));
    size_t accepted=99;CHECK(xl6ExecutionUdpSendBatchV1(graph.sender,NULL,0,&accepted)==XNET_RESULT_ERROR&&accepted==0);xrtClearError();
    accepted=99;CHECK(xl6ExecutionUdpSendOwnedBatchV1(graph.sender,NULL,0,&accepted)==XNET_RESULT_ERROR&&accepted==0);xrtClearError();
    void* taken=xrtMalloc(1);CHECK(taken);
    CHECK(xl6ExecutionUdpSendTakeV1(graph.sender,NULL,NULL,taken,1)==XNET_RESULT_ERROR);xrtClearError();xrtFree(taken);
    #ifdef UDP_FUTURE_ACCEPTANCE
    xfuture* closed=xl6ExecutionUdpWaitAsyncV1(graph.sender,XNET_UDP_WAIT_CLOSE);CHECK(closed);
    CHECK(xrtFutureWaitFor(closed,UINT64_C(5000000))==XWAIT_OK);xrtFutureDestroy(closed);
    #endif
    /* Last user owner disappears in a physical call scope: retirement waits
     * for Leave, while new entries correctly fail instead of using stale UDP. */
    XL6_UdpScopeV1 scope={0};CHECK(xl6ExecutionUdpEnterInternalV1(graph.receiver,false,&scope));
    xl6ExecutionResourceReleaseV1(graph.receiver);graph.receiver=NULL;
    CHECK(xrtNetUdpState(scope.udp)==XNET_UDP_OPEN);xl6ExecutionUdpLeaveInternalV1(&scope);
    dropOrdinary(&graph);CHECK(xrtNetUdpPacketSize(survivor)==3&&!memcmp(xrtNetUdpPacketData(survivor),"p\0q",3));
    xrtNetUdpPacketDestroy(survivor);balanced(&before);
}
#ifdef UDP_FUTURE_ACCEPTANCE
static void futureCases(Graph* graph,unsigned state)
{
    if(state==0)return;
    if(state==1)graph->pending=xl6ExecutionUdpReceiveAsyncV1(graph->receiver);
    if(state==2)graph->pending=xl6ExecutionUdpReceiveBatchAsyncV1(graph->receiver,4);
    if(state==3)graph->pending=xl6ExecutionUdpWaitAsyncV1(graph->sender,XNET_UDP_WAIT_CLOSE);
    CHECK(graph->pending&&xrtFutureState(graph->pending)==XFUTURE_PENDING);
}
static void resolved(Graph* graph)
{
    xfuture* future=xl6ExecutionUdpWritableAsyncV1(graph->sender,3);CHECK(future);
    CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);xrtFutureDestroy(future);
    future=xl6ExecutionUdpReceiveBatchAsyncV1(graph->receiver,4);CHECK(future);
    CHECK(xl6ExecutionUdpSendV1(graph->sender,NULL,NULL,"a\0b",3)==XNET_RESULT_OK);
    CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
    xnetudpbatch* batch=xrtFutureValue(future);CHECK(batch&&xrtNetUdpBatchCount(batch)==1);
    xnetudppacket* packet=xrtNetUdpBatchTake(batch,0);CHECK(packet&&xrtNetUdpPacketSize(packet)==3);
    xrtFutureDestroy(future);CHECK(!memcmp(xrtNetUdpPacketData(packet),"a\0b",3));xrtNetUdpPacketDestroy(packet);
    future=xl6ExecutionUdpReceiveAsyncV1(graph->receiver);CHECK(future&&xrtFutureCancel(future));
    CHECK(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_CANCELLED);xrtFutureDestroy(future);
}
#endif
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
    xl6_error_result error=xl6ErrorScopeEnd(&scope);CHECK(error.ok);xrtErrorFree(error.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "udp-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&strcmp(argv[1],"native")==0){
        #if defined(_WIN32) || defined(_WIN64)
        backend=XNET_PORT_IOCP;
        #elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
        #endif
    }
    unsigned states=1;
    #ifdef UDP_FUTURE_ACCEPTANCE
    states=4;
    #endif
    for(unsigned state=0;state<states;++state)for(unsigned order=0;order<9;++order){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend);roundTrip(&graph);
        #ifdef UDP_FUTURE_ACCEPTANCE
        resolved(&graph);futureCases(&graph,state);
        #endif
        if(order<8){collect(&graph,order);++graphs;}else{
            #ifdef UDP_FUTURE_ACCEPTANCE
            xfuture* pending=graph.pending;
            #endif
            dropOrdinary(&graph);
            #ifdef UDP_FUTURE_ACCEPTANCE
            if(pending){CHECK(xrtFutureState(pending)!=XFUTURE_PENDING);xrtFutureDestroy(pending);}
            #endif
            ++ordinary;
        }balanced(&before);
    }
    construction(backend);lifecycle(backend);CHECK(failures>0&&failures<budgets);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("managed UDP resources: graphs=%u ordinary=%u allocation-budgets=%u failures=%u; typed sends, NUL roundtrip; balanced backend=%d\n",
        graphs,ordinary,budgets,failures,(int)backend);return 0;
}
