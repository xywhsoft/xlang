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
#include "src/runtime/xl6_udp_future_internal.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_continuation_future.h"
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
static XL6_ContinuationExecutor* executor;
static xfuture* results[2];static size_t resultCount;
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
        graph->busy=false;xrtownershipref anchors[CAP+ROOTS+5],slots[CAP+ROOTS+4];size_t a=0,s=0;
        anchors[a++]=xl6ExecutionSourceOwnershipV1(graph->source);slots[s++]=anchors[0];
        anchors[a++]=xl6ExecutionResourceOwnershipV1(graph->engine); // only children own Engine
        for(size_t i=0;i<graph->rootsCount;++i){anchors[a++]=xrtValueOwnership(graph->roots[i]);slots[s++]=anchors[a-1];}
        if(graph->pinned)for(size_t i=0;i<graph->count;++i){anchors[a++]=graph->nodes[i].ref;slots[s++]=anchors[a-1];}
        anchors[a++]=xl6ContinuationExecutorOwnership(executor);slots[s++]=anchors[a-1];
        for(size_t i=0;i<resultCount;++i){anchors[a++]=xrtFutureOwnership(results[i]);slots[s++]=anchors[a-1];}
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
    for(size_t i=0;i<resultCount;++i){xrtFutureDestroy(results[i]);results[i]=NULL;}resultCount=0;
    CHECK(xl6ContinuationExecutorDestroyCompleted(executor));executor=NULL;
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

static const XL6_ClassIdentitySchemaV3 schemas[]={
    {sizeof(XL6_ClassIdentitySchemaV3),101,101,XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}},
    {sizeof(XL6_ClassIdentitySchemaV3),102,102,XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}},
    {sizeof(XL6_ClassIdentitySchemaV3),103,103,XL6_CLASS_IDENTITY_REFERENCE_V3,XL6_CLASS_IDENTITY_REFERENCE_V3,NULL,0,{0}}};
static XL6_NativeClassResultContextV1 context(unsigned kind)
{
    return (XL6_NativeClassResultContextV1){{"handle",6,kind==0?xl6UdpPacketHandlePolicyV1Get():
        kind==1?xl6UdpErrorPacketHandlePolicyV1Get():xl6UdpBatchHandlePolicyV1Get()},NULL,&schemas[kind]};
}
static void awaitResult(xfuture* future,unsigned kind)
{
    CHECK(future&&xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
    xvalue* const* box=xrtFutureValue(future);CHECK(box&&*box&&xrtValueTypeId(*box)==schemas[kind].identity);
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    CHECK(xrtValueGetHandle(xrtValueObjectGet(*box,(xstrview){"handle",6}),&raw,&ops,&user)&&raw&&!user&&ops==context(kind).field.policy->Ops);
    if(kind==0)packet(raw,"z\0p",3);
    if(kind==1){xvalue* value=xl6UdpErrorPacketBytesV1(raw,-1,0);view(value,"e\0r",3);xrtValueRelease(value);}
    if(kind==2)CHECK(xl6UdpBatchCountV1(raw)==1);
}
static void launch(Graph* graph,unsigned kind)
{
    executor=xl6ContinuationExecutorCreateFixedV1(64,2);CHECK(executor);
    XL6_RuntimeServicesV1 services={executor,NULL};resultCount=1;
    if(kind==0||kind==4)results[0]=xl6UdpReceiveAsyncV1(services,context(0),graph->receiver);
    if(kind==1||kind==5)results[0]=xl6UdpReceiveBatchAsyncV1(services,context(2),graph->receiver,4);
    if(kind==2)results[0]=xl6UdpWaitAsyncV1(services,graph->receiver,XNET_UDP_WAIT_CLOSE);
    if(kind==3){
        char name[7]="handle";XL6_NativeClassResultContextV1 c=context(1);c.field.fieldName=name;
        XL6_UdpFutureCaptureV1* capture=xl6UdpFuturePrepareInternalV1(XL6_UDP_FUTURE_ERROR,graph->receiver,0,&c);CHECK(capture);
        memset(name,'x',6);xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);CHECK(promise&&source);
        void* value=udpFixtureError();CHECK(value&&xrtPromiseResolveOwnedPolicyV1(promise,value,xrtNetUdpFutureErrorPayloadPolicyV1Get()));
        xrtPromiseDestroy(promise);results[0]=xl6UdpFuturePublishInternalV1(services,capture,source);
        awaitResult(results[0],1);
    }
    if(kind==4||kind==5){
        CHECK(xl6UdpSendTextV1(graph->sender,(xstrview){"z\0p",3},-1)==XNET_RESULT_OK);
        awaitResult(results[0],kind==4?0:2);
    }
    if(kind==6){
        xcancel* parent=xrtCancelCreate();CHECK(parent);
        results[0]=xl6UdpReceiveAsyncCancelV1(services,context(0),graph->receiver,parent);
        xrtCancelDestroy(parent); // only the actual frame's owned Watch remains
    }
    CHECK(results[0]);
}
typedef struct LeaseOwner {unsigned refs,acquires,releases;bool closed;} LeaseOwner;
static int leaseHold(void* raw){LeaseOwner* owner=raw;if(owner->closed)return 0;++owner->refs;++owner->acquires;return 1;}
static void leaseDrop(void* raw){LeaseOwner* owner=raw;CHECK(owner->refs);--owner->refs;++owner->releases;}
static void afterHostClose(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend);
    executor=xl6ContinuationExecutorCreateFixedV1(16,1);CHECK(executor);XL6_RuntimeServicesV1 services={executor,NULL};
    LeaseOwner owner={0};XL6_CodeOwner code={sizeof(code),&owner,leaseHold,leaseDrop};XL6_NativeClassResultContextV1 c=context(1);c.codeOwner=&code;
    XL6_UdpFutureCaptureV1* capture=xl6UdpFuturePrepareInternalV1(XL6_UDP_FUTURE_ERROR,graph.receiver,0,&c);CHECK(capture&&owner.acquires==1);
    xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);CHECK(promise&&source);
    xfuture* result=xl6UdpFuturePublishInternalV1(services,capture,source);CHECK(result);owner.closed=true;
    void* error=udpFixtureError();CHECK(error&&xrtPromiseResolveOwnedPolicyV1(promise,error,xrtNetUdpFutureErrorPayloadPolicyV1Get()));xrtPromiseDestroy(promise);
    awaitResult(result,1);xl6ContinuationExecutorClose(executor);
    CHECK(xl6ContinuationExecutorDestroyCompleted(executor));executor=NULL;
    CHECK(owner.refs==1&&owner.acquires==1);xrtFutureDestroy(result);CHECK(!owner.refs&&owner.acquires==owner.releases);
    for(size_t i=0;i<graph.rootsCount;++i)xrtValueRelease(graph.roots[i]);
    CHECK(xl6ExecutionSourceCloseV1(graph.source));
    XL6_DeadlineV1 until=xl6DeadlineAfterMillisecondsV1(5000);for(;;){XL6_ExecutionRetireResultV1 retired=xl6ExecutionSourceDestroyV1(&graph.source);CHECK(retired.ok);if(retired.ready)break;CHECK(!xl6DeadlineExpiredV1(until));xrtThreadYield();}
    balanced(&before);
}
/* Real queue/Watch owners with deterministic inspection INSIDE Notify, before
 * XRT returns the transport credit. No sleeps, fabricated counts or OS traffic. */
typedef struct PhaseStorage {XL6_ContinuationTicket ticket;unsigned refs,ran,dropped,observed;} PhaseStorage;
static bool phaseCount(const void* raw,size_t* count){const PhaseStorage* s=raw;if(!s->refs)return false;*count=s->refs;return true;}
static bool phaseTrace(const void* raw,xrtownershipvisitor visit,void* data){(void)raw;(void)data;return visit!=NULL;}
static const xrtownershipops phaseOps={phaseCount,phaseTrace};
static bool phaseHold(const void* raw){++((PhaseStorage*)raw)->refs;return true;}
static void phaseDrop(const void* raw){PhaseStorage* s=(PhaseStorage*)raw;CHECK(s->refs);--s->refs;}
static xrtownershipref phaseOwnership(const void* raw){return (xrtownershipref){raw,&phaseOps};}
static const XL6_ContinuationStorageV2 phaseStorage;
static void phaseInspect(PhaseStorage* s,unsigned flags)
{
    CHECK(s->ticket.flags==flags&&xl6ContinuationTicketInspectableV1(&s->ticket));
    CHECK(s->refs==s->ticket.ownershipRefs);
    xrtownershipscope freeze={0};CHECK(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref queue=xl6ContinuationQueueOwnership(s->ticket.queue);size_t count=0;
    CHECK(queue.Ops->Count(queue.Data,&count)&&count==1);
    const xrtownershippreparationv1* prepare=NULL;
    const XL6_ContinuationStorageV2* known[]={&phaseStorage};
    CHECK(xl6ContinuationQueueOwnershipAdapterV1(queue,known,1,&prepare)&&prepare);
    CHECK(!xl6ContinuationQueueOwnershipAdapterV1(queue,NULL,0,&prepare));
    xrtownershipsnapshot* graph=NULL;
    CHECK(xrtOwnershipSnapshotCreate(&queue,1,NULL,0,NULL,NULL,&graph));
    CHECK(xrtOwnershipSnapshotNodeCount(graph)==2);xrtOwnershipSnapshotDestroy(graph);
    CHECK(xrtOwnershipScopeEnd(&freeze));++s->observed;
    XL6_ContinuationTicket invalid=s->ticket;
    const unsigned rejected[]={0,8,16,32,1|8,1|16,1|32,1|64};
    for(size_t i=0;i<sizeof(rejected)/sizeof(*rejected);++i){invalid.flags=rejected[i];CHECK(!xl6ContinuationTicketInspectableV1(&invalid));}
    invalid=s->ticket;invalid.predecessor=&s->ticket;CHECK(!xl6ContinuationTicketInspectableV1(&invalid));
    invalid=s->ticket;invalid.successor=&s->ticket;CHECK(!xl6ContinuationTicketInspectableV1(&invalid));
}
static void phaseReady(void* raw){PhaseStorage* s=raw;xl6ContinuationReady(&s->ticket);phaseInspect(s,3);}
static void phaseReleased(void* raw){xl6ContinuationTransportReleased(&((PhaseStorage*)raw)->ticket);}
static bool phaseWatch(const void* raw,xfuturewatch* watch)
{static const xfuturewatchownershipv1 p={sizeof(p),phaseReady,phaseReleased,&phaseOps};return xrtFutureWatchInitOwnershipV1(watch,(void*)raw,&p);}
static const XL6_ContinuationStorageV2 phaseStorage={phaseHold,phaseDrop,phaseOwnership,phaseWatch,NULL,NULL,NULL,NULL};
static void phaseRun(void* raw){PhaseStorage* s=raw;CHECK(!xl6ContinuationTicketInspectableV1(&s->ticket));++s->ran;CHECK(xl6ContinuationComplete(&s->ticket));}
static void phaseRetire(void* raw){++((PhaseStorage*)raw)->dropped;}
static void ticketPhases(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(unsigned reverse=0;reverse<2;++reverse){
        XL6_ContinuationQueue* queue=xl6ContinuationQueueCreate(2);CHECK(queue);PhaseStorage s={.refs=1};
        CHECK(xl6ContinuationReserve(queue,&s.ticket,phaseRun,phaseRetire,&s)==XL6_CONTINUATION_ADMITTED);
        /* Private resident storage binding, before transport registration. */
        s.ticket.storage=&phaseStorage;xfuture* source=NULL;xpromise* promise=NULL;xfuturewatch watch={0};
        if(!reverse){promise=xrtPromiseCreate(&source,NULL);CHECK(promise&&source);CHECK(xl6ContinuationWatchRegister(&s.ticket,&watch,source));}
        xl6ContinuationCommit(&s.ticket);phaseInspect(&s,1);
        if(!reverse){CHECK(xrtPromiseResolve(promise,NULL));xrtPromiseDestroy(promise);xrtFutureDestroy(source);}
        else{xl6ContinuationTransportReleased(&s.ticket);phaseInspect(&s,5);xl6ContinuationReady(&s.ticket);}
        phaseInspect(&s,7);CHECK(s.observed==3);CHECK(xl6ContinuationRunOne(queue));
        CHECK(!s.refs&&s.ran==1&&s.dropped==1);xl6ContinuationQueueClose(queue);CHECK(xl6ContinuationQueueDestroy(queue));
    }
    balanced(&before);
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope warm={0};CHECK(xl6ErrorScopeBegin(&warm));endOk(&warm);
    const XL6_OwnershipDomainV1* domains[]={
        #include "udp-language-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));CHECK(catalog);ticketPhases();
    xnetportbackend backend=XNET_PORT_SELECT;if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned kind=0;kind<7;++kind)for(unsigned order=0;order<8;++order){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend);
        launch(&graph,kind);collect(&graph,order);balanced(&before);
    }
    afterHostClose(backend);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("UDP async runtime: graphs=%u; real class/native/queued-frame/executor ownership, class publication after host Close, offline error fixture; balanced\n",graphs);return 0;
}

