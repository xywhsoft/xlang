/* Actual managed handles and native graph. Normal literal loopback traffic
 * only; error-packet fields are tested using an offline native fixture. */
#include "release/include/xl_clock.h"
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_execution_udp_internal.h"
#include "src/runtime/xl6_execution_net_engine.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
extern xnetudppacket* udpFixturePacket(const xnetaddr*);
extern xnetudperrorpacket* udpFixtureError(void);
enum {CAP=160,ROOTS=12};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* prepare;const xrtownershipfinishv1* finish;bool ready,done;} Node;
typedef struct Graph {XL6_ExecutionSourceV1* source;XL6_ExecutionResourceV1* engine;
    void *sender,*receiver;xvalue* roots[ROOTS];size_t rootsCount;Node nodes[CAP];size_t count;bool pinned,busy;} Graph;
static XL6_OwnershipCatalog* catalog;
static unsigned graphs,roundtrips,oom;
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void endOk(xl6_error_scope* scope)
{xl6_error_result result=xl6ErrorScopeEnd(scope);CHECK(result.ok&&!result.error);}
static void endError(xl6_error_scope* scope,xerrkind expected)
{xl6_error_result result=xl6ErrorScopeEnd(scope);if(result.ok||(expected&&xrtErrorKind(result.error)!=expected))fprintf(stderr,"checked expected=%d actual=%d ok=%d\n",(int)expected,(int)xrtErrorKind(result.error),(int)result.ok);CHECK(!result.ok&&(!expected||xrtErrorKind(result.error)==expected));xrtErrorFree(result.error);}
static xvalue* own(Graph* graph,void* raw,const XL6_NativeHandlePolicyV1* policy)
{xvalue* value=xl6NativeHandleTakeV1(&raw,policy);CHECK(value&&!raw&&graph->rootsCount<ROOTS);graph->roots[graph->rootsCount++]=value;return value;}
static void view(const xvalue* value,const void* data,size_t size)
{xbytesview bytes={0};CHECK(value&&xrtValueGetBytes(value,&bytes)&&bytes.Size==size&&(!size||!memcmp(bytes.Data,data,size)));}
static void packet(void* raw,const void* data,size_t size)
{CHECK(raw&&xl6UdpPacketLengthV1(raw)==size&&!xl6UdpPacketTruncatedV1(raw));xvalue* value=xl6UdpPacketBytesV1(raw,-1,0);view(value,data,size);xrtValueRelease(value);}
static void receive(Graph* graph,const void* data,size_t size)
{void* raw=xl6UdpReceiveV1(graph->receiver,5000);packet(raw,data,size);xl6UdpPacketDestroyV1(raw);++roundtrips;}
static bool admit(xrtownershipref ref,void* raw)
{
    Graph* graph=raw;const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const xrtownershipfinishv1* finish=NULL;
    XL6_OwnershipAdmissionV1 result=xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&prepare,&finish);
    if(result==XL6_OWNERSHIP_ADMISSION_BUSY)graph->busy=true;
    return result==XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
static xrtownershipsnapshot* snapshot(Graph* graph,xrtownershipscope* frozen)
{
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){
        while(!xrtOwnershipFreezeTryBegin(frozen)){CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
        graph->busy=false;xrtownershipref anchors[CAP+ROOTS+2],slots[CAP+ROOTS+1];size_t a=0,s=0;
        anchors[a++]=xl6ExecutionSourceOwnershipV1(graph->source);slots[s++]=anchors[0];
        anchors[a++]=xl6ExecutionResourceOwnershipV1(graph->engine); // only children own Engine
        for(size_t i=0;i<graph->rootsCount;++i){anchors[a++]=xrtValueOwnership(graph->roots[i]);slots[s++]=anchors[a-1];}
        if(graph->pinned)for(size_t i=0;i<graph->count;++i){anchors[a++]=graph->nodes[i].ref;slots[s++]=anchors[a-1];}
        xrtownershipsnapshot* proof=NULL;if(xrtOwnershipSnapshotCreate(anchors,a,slots,s,admit,graph,&proof))return proof;
        CHECK(graph->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(frozen)&&!xl6DeadlineExpiredV1(until));xrtThreadYield();
    }
}
static void discover(Graph* graph,xrtownershipsnapshot* proof)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);size_t j=0;
        while(j<graph->count&&(graph->nodes[j].ref.Data!=item.Reference.Data||graph->nodes[j].ref.Ops!=item.Reference.Ops))++j;
        if(j<graph->count)continue;
        CHECK(graph->count<CAP);Node* node=&graph->nodes[graph->count++];node->ref=item.Reference;
        CHECK(xl6OwnershipCatalogResolveV1(node->ref,catalog,&node->adapter,&node->prepare,&node->finish));
        CHECK(node->adapter->Hold(node->ref.Data)&&node->adapter->Claim(node->ref.Data,graph));
    }graph->pinned=true;
}
static void collect(Graph* graph,unsigned order)
{
    xrtownershipscope frozen={0};xrtownershipsnapshot* proof=snapshot(graph,&frozen);discover(graph,proof);
    xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&frozen));CHECK(graph->count>graph->rootsCount+4);
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){bool ready=true;
        for(size_t step=0;step<graph->count;++step){size_t i=order&1?graph->count-1-step:step;Node* node=&graph->nodes[i];if(!node->prepare||node->ready)continue;
            proof=snapshot(graph,&frozen);discover(graph,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&frozen));
            xrtownershipprepareresult result=node->prepare->Prepare(node->ref.Data,graph);CHECK(result!=XRT_OWNERSHIP_PREPARE_FAILED);
            node->ready=result==XRT_OWNERSHIP_PREPARE_READY;ready=ready&&node->ready;}
        if(ready)break;
        CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();
    }
    proof=snapshot(graph,&frozen);discover(graph,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<graph->count;++i){Node* node=&graph->nodes[i];CHECK(!node->prepare||node->prepare->Ready(node->ref.Data));node->adapter->Clear(node->ref.Data,graph);}
    CHECK(xrtOwnershipScopeEnd(&frozen));until=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){bool done=true;
        for(size_t step=0;step<graph->count;++step){size_t i=order&2?graph->count-1-step:step;Node* node=&graph->nodes[i];if(node->done)continue;
            if(node->finish){xrtownershipfinishresult result=node->finish->Step(node->ref.Data,graph);CHECK(result!=XRT_OWNERSHIP_FINISH_FAILED);node->done=result==XRT_OWNERSHIP_FINISH_READY;}
            else node->done=!node->adapter->Finish||node->adapter->Finish(node->ref.Data,graph);
            done=done&&node->done;}
        if(done)break;
        CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();
    }
    for(size_t i=0;i<graph->rootsCount;++i)xrtValueRelease(graph->roots[i]);
    XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&graph->source);CHECK(result.ok&&result.ready&&!graph->source);
    for(size_t step=0;step<graph->count;++step){size_t i=order&4?graph->count-1-step:step;graph->nodes[i].adapter->Drop(graph->nodes[i].ref.Data);}
    ++graphs;
}
static void create(Graph* graph,xnetportbackend backend)
{
    graph->source=xl6ExecutionSourceCreateV1();CHECK(graph->source);xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=2;config.Backend=backend;
    graph->engine=xl6ExecutionNetEngineCreateV1(graph->source,&config);CHECK(graph->engine&&xl6ExecutionNetEngineStartV1(graph->engine));
    xnetaddr local;CHECK(xrtNetAddrLoopback(&local,XNET_FAMILY_IPV4,0));XL6_ExecutionServicesV1 services={graph->source};
    graph->receiver=xl6UdpBindV1(services,graph->engine,&local,NULL,UINT64_MAX);CHECK(graph->receiver);
    CHECK(xl6UdpWaitV1(graph->receiver,XNET_UDP_WAIT_OPEN,5000)==XNET_RESULT_OK);own(graph,graph->receiver,xl6UdpHandlePolicyV1Get());
    void* address=xl6UdpLocalV1(graph->receiver);CHECK(address);
    graph->sender=xl6UdpConnectV1(services,graph->engine,address,NULL,UINT64_MAX);xl6NetAddressDestroyV1(address);CHECK(graph->sender);
    CHECK(xl6UdpWaitV1(graph->sender,XNET_UDP_WAIT_OPEN,5000)==XNET_RESULT_OK);own(graph,graph->sender,xl6UdpHandlePolicyV1Get());
    CHECK(xl6UdpConnectedV1(graph->sender)&&!xl6UdpConnectedV1(graph->receiver));xl6ExecutionResourceReleaseV1(graph->engine);
}
static void exercise(Graph* graph)
{
    CHECK(!xl6UdpTryReceiveV1(graph->receiver));
    CHECK(xl6UdpWaitV1(graph->receiver,XNET_UDP_WAIT_RECEIVE,0)==XNET_RESULT_TIMEOUT);
    CHECK(xl6UdpWaitWritableV1(graph->sender,0,5000)==XNET_RESULT_OK);
    CHECK(xl6UdpSendTextV1(graph->sender,(xstrview){"a\0b",3},100)==XNET_RESULT_OK);receive(graph,"a\0b",3);
    xvalue* bytes=xrtValueBytes((xbytesview){(const unsigned char*)"_x\0y_",5});CHECK(bytes);
    CHECK(xl6UdpSendBytesV1(graph->sender,bytes,3)==XNET_RESULT_OK);xrtValueRelease(bytes);receive(graph,"_x\0",3);
    xbuffer buffer={0};CHECK(xrtBufferAppend(&buffer,(xbytesview){(const unsigned char*)"copy",4}));
    CHECK(xl6UdpSendBufferV1(graph->sender,&buffer,3)==XNET_RESULT_OK&&buffer.Size==4);receive(graph,"cop",3);
    CHECK(xl6UdpSendMoveV1(graph->sender,&buffer)==XNET_RESULT_OK&&!buffer.Size&&!buffer.Data&&!buffer.Capacity);receive(graph,"copy",4);
    CHECK(xrtBufferReserve(&buffer,128));CHECK(xl6UdpSendMoveV1(graph->sender,&buffer)==XNET_RESULT_OK&&!buffer.Data);receive(graph,NULL,0);
    xnetbytes* input=xrtNetBytesCopy("_r\0f_",5);CHECK(input);
    CHECK(xl6UdpSendRefBytesV1(graph->sender,input,100,1)==XNET_RESULT_OK);xrtNetBytesDestroy(input);receive(graph,"r\0f_",4);
    xnetaddr remote;CHECK(xrtNetAddrLoopback(&remote,XNET_FAMILY_IPV4,0));void* offline=udpFixturePacket(&remote);CHECK(offline);
    CHECK(xl6UdpSendRefPacketV1(graph->sender,offline,3,1)==XNET_RESULT_OK);packet(offline,"_p\0q_",5);receive(graph,"p\0q",3);
    own(graph,offline,xl6UdpPacketHandlePolicyV1Get());void* error=udpFixtureError();CHECK(error&&xl6UdpErrorPacketLengthV1(error)==3);
    bytes=xl6UdpErrorPacketBytesV1(error,100,1);view(bytes,"\0r",2);xrtValueRelease(bytes);own(graph,error,xl6UdpErrorPacketHandlePolicyV1Get());
    void* info=xl6UdpErrorPacketInfoV1(error);CHECK(info);xl6NetDatagramErrorInfoDestroyV1(info);
    void* peer=xl6UdpPeerV1(graph->sender);CHECK(peer);xl6NetAddressDestroyV1(peer);
    CHECK(xl6UdpSendTextV1(graph->sender,(xstrview){"batch",5},-1)==XNET_RESULT_OK);
    void* batch=xl6UdpReceiveBatchV1(graph->receiver,8,5000);CHECK(batch&&xl6UdpBatchCountV1(batch)==1);
    void* taken=xl6UdpBatchTakeV1(batch,0);packet(taken,"batch",5);CHECK(!xl6UdpBatchTakeV1(batch,0)&&xl6UdpBatchCountV1(batch)==1);
    own(graph,taken,xl6UdpPacketHandlePolicyV1Get());own(graph,batch,xl6UdpBatchHandlePolicyV1Get());
    info=xl6UdpPacketInfoV1(taken);CHECK(info);xl6NetDatagramInfoDestroyV1(info);
    void* stats=xl6UdpSnapshotV1(graph->receiver);CHECK(stats&&xl6UdpStatsStateV1(stats)==XNET_UDP_OPEN);
    CHECK(xl6UdpStatsReceivedPacketsV1(stats)>=8);own(graph,stats,xl6UdpStatsHandlePolicyV1Get());
    #define XL6_UDP_STAT(Field,Name) (void)xl6UdpStats##Field##V1(stats);
    #include "src/runtime/xl6_udp_stats_fields.h"
    #undef XL6_UDP_STAT
    xl6_error_scope scope={0};
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(!xl6UdpBatchTakeV1(batch,-1));endError(&scope,XERR_RANGE);
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(!xl6UdpPacketBytesV1(offline,-2,0));endError(&scope,XERR_RANGE);
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(!xl6UdpPacketBytesV1(offline,0,6));endError(&scope,XERR_RANGE);
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(!xl6UdpReceiveBatchV1(graph->receiver,257,0));endError(&scope,XERR_RANGE);
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(xl6UdpWaitWritableV1(graph->sender,-1,0)==XNET_RESULT_ERROR);endError(&scope,XERR_RANGE);
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(!xl6UdpReceiveV1(graph->receiver,0));endError(&scope,XERR_TIMEOUT);
    xcancel* cancel=xrtCancelCreate();CHECK(cancel&&xrtCancelRequest(cancel));
    CHECK(xl6UdpWaitCancelV1(graph->receiver,cancel,XNET_UDP_WAIT_RECEIVE,5000)==XNET_RESULT_CANCELLED);
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(!xl6UdpReceiveCancelV1(graph->receiver,cancel,5000));endError(&scope,XERR_CANCELLED);xrtCancelDestroy(cancel);
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(!xl6UdpReceiveCancelV1(graph->receiver,NULL,0));endError(&scope,XERR_ARGUMENT);
    CHECK(xrtBufferAppend(&buffer,(xbytesview){(const unsigned char*)"keep",4}));xl6UdpCloseV1(graph->sender);
    CHECK(xl6ErrorScopeBegin(&scope));CHECK(xl6UdpSendMoveV1(graph->sender,&buffer)==XNET_RESULT_ERROR);endError(&scope,XERR_STATE);
    CHECK(buffer.Size==4&&!memcmp(buffer.Data,"keep",4));xrtBufferUnit(&buffer);
}
static void offlineOom(void)
{
    xnetaddr remote;CHECK(xrtNetAddrLoopback(&remote,XNET_FAMILY_IPV4,0));void* raw=udpFixturePacket(&remote);CHECK(raw);
    for(unsigned budget=0;budget<12;++budget){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(budget));
        xvalue* value=xl6UdpPacketBytesV1(raw,-1,0);bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);CHECK((value!=NULL)==outcome.ok&&(value!=NULL)==!triggered);
        xrtValueRelease(value);xrtErrorFree(outcome.error);balanced(&before);++oom;
    }xl6UdpPacketDestroyV1(raw);
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope warm={0};CHECK(xl6ErrorScopeBegin(&warm));endOk(&warm);
    const XL6_OwnershipDomainV1* domains[]={
        #include "udp-language-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xnetportbackend backend=XNET_PORT_SELECT;if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned order=0;order<8;++order){xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};
        create(&graph,backend);exercise(&graph);collect(&graph,order);balanced(&before);}
    offlineOom();xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("UDP language runtime: graphs=%u roundtrips=%u offline-OOM=%u; exact policies, batch take, cancel, move rollback; balanced\n",graphs,roundtrips,oom);return 0;
}
