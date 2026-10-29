/* XRT dbd5f71c fixture; only bootstrap and pinned millisecond deadlines adapted. */
/* Owned Dial/Future graph and valid own-loopback only. No DNS, external
 * endpoint, protocol attacks, malformed traffic or security tests. */
#define XRT_IMPLEMENTATION
#include "release/include/xl_clock.h"
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool condition,const char* message)
{if(!condition){fprintf(stderr,"[FAIL] %s\n",message);exit(1);}}
#define CHECK(C) do { if (!(C)) { fprintf(stderr,"line %u: ",(unsigned)__LINE__);testRequire(false,#C); } } while(0)
typedef struct State {
    xatomic32 references,gate,entered,done,cleared;
    xatomicptr client;
    xnetresult result;
    const void* claim;
} State;
typedef struct Node {
    xrtownershipref ref;
    const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* preparation;
    bool ready;
} Node;
typedef struct Graph {
    xnetengine* engine;xnetresolver* resolver;xnetlistener* listener;
    xnetdial* dial;xfuture* future;xnetstream* server;State* state;
    Node nodes[64];size_t count;bool busy,pinned;unsigned wrong;
} Graph;
static unsigned graphs,ordinary,pending,refusals,frames,candidates,budgets,allocationFailures;
static bool stateHold(const void* raw)
{
    State* state=(State*)raw;xrtownershipscope mutation={0};CHECK(xrtOwnershipMutationBegin(&mutation));
    bool held=!xrtAtomic32Load(&state->cleared,XMEMORY_ACQUIRE);
    if(held)CHECK(xrtAtomic32FetchAdd(&state->references,1,XMEMORY_ACQ_REL)>0);
    CHECK(xrtOwnershipScopeEnd(&mutation));return held;
}
static void stateDrop(const void* raw)
{
    State* state=(State*)raw;xrtownershipscope mutation={0};CHECK(xrtOwnershipMutationBegin(&mutation));
    uint32 before=xrtAtomic32FetchSub(&state->references,1,XMEMORY_ACQ_REL);CHECK(before);
    CHECK(xrtOwnershipScopeEnd(&mutation));if(before!=1)return;
    xnetstream* stream=xrtAtomicPtrLoad(&state->client,XMEMORY_ACQUIRE);
    if(stream){CHECK(xrtNetStreamAbort(stream));xrtNetStreamDestroy(stream);}
    xrtFree(state);
}
static bool stateCount(const void* raw,size_t* count)
{
    const State* state=raw;if(!state||!count||xrtAtomic32Load(&state->cleared,XMEMORY_ACQUIRE))return false;
    *count=xrtAtomic32Load(&state->references,XMEMORY_ACQUIRE);return *count>0;
}
static bool stateTrace(const void* raw,xrtownershipvisitor visit,ptr data)
{
    const State* state=raw;size_t count;if(!visit||!stateCount(state,&count))return false;
    xnetstream* stream=xrtAtomicPtrLoad(&state->client,XMEMORY_ACQUIRE);
    return !stream||visit(xrtNetStreamOwnership(stream),data);
}
static const xrtownershipops stateOps={stateCount,stateTrace};
static bool stateClaim(const void* raw,const void* token)
{
    State* state=(State*)raw;if(!token||(state->claim&&state->claim!=token))return false;
    state->claim=token;return true;
}
static void stateRestore(const void* raw,const void* token)
{State* state=(State*)raw;CHECK(token&&state->claim==token);state->claim=NULL;}
static void stateClear(const void* raw,const void* token)
{
    State* state=(State*)raw;CHECK(token&&state->claim==token&&!xrtAtomic32Load(&state->cleared,XMEMORY_ACQUIRE));
    xnetstream* stream=xrtAtomicPtrExchange(&state->client,NULL,XMEMORY_ACQ_REL);
    xrtAtomic32Store(&state->cleared,1,XMEMORY_RELEASE);xrtNetStreamDestroy(stream);
}
static const xrtownershipadapterv1 stateAdapter={sizeof(stateAdapter),stateHold,stateDrop,stateClaim,stateRestore,NULL,stateClear,NULL};
static xnetaddrlist* lookup(cstr host,xnetfamily family,ptr raw)
{
    (void)host;(void)family;State* state=raw;
    xrtAtomic32Store(&state->entered,1,XMEMORY_RELEASE);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
    while(!xrtAtomic32Load(&state->gate,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    return xrtNetAddrListCreate(&address,1);
}
static const xnetresolverlookupownershipv1 lookupPolicy={sizeof(lookupPolicy),lookup,stateDrop,&stateOps};
static void done(xnetdial* dial,xnetresult result,xnetstream* stream,const xerror* error,ptr raw)
{
    (void)dial;(void)error;State* state=raw;xrtownershipscope mutation={0};CHECK(xrtOwnershipMutationBegin(&mutation));
    CHECK(!xrtAtomicPtrLoad(&state->client,XMEMORY_ACQUIRE));
    xrtAtomicPtrStore(&state->client,stream,XMEMORY_RELEASE);state->result=result;
    xrtAtomic32Store(&state->done,1,XMEMORY_RELEASE);CHECK(xrtOwnershipScopeEnd(&mutation));
}
static const xnetdialownershipv1 donePolicy={sizeof(donePolicy),done,stateDrop,&stateOps};
static void freeze(xrtownershipscope* scope)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
    while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void create(Graph* graph,xnetportbackend backend,bool gate,bool resolverEnabled)
{
    graph->state=xrtCalloc(1,sizeof(*graph->state));CHECK(graph->state);
    xrtAtomic32Init(&graph->state->references,1);xrtAtomic32Init(&graph->state->gate,gate?1:0);
    xrtAtomic32Init(&graph->state->entered,0);xrtAtomic32Init(&graph->state->done,0);
    xrtAtomic32Init(&graph->state->cleared,0);xrtAtomicPtrInit(&graph->state->client,NULL);
    xnetengineconfig engine;xrtNetEngineConfigInit(&engine);engine.Workers=1;engine.Backend=backend;
    graph->engine=xrtNetEngineCreate(&engine);CHECK(graph->engine&&xrtNetEngineStart(graph->engine));
    if(resolverEnabled){
        xnetresolverconfig resolver;xrtNetResolverConfigInit(&resolver);resolver.Workers=1;resolver.CacheEntries=0;
        CHECK(stateHold(graph->state));graph->resolver=xrtNetResolverCreateOwnedV1(&resolver,graph->state,&lookupPolicy);CHECK(graph->resolver);
    }
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    graph->listener=xrtNetListen(graph->engine,&listen,NULL,NULL,NULL);CHECK(graph->listener);
}
static const xrtownershipadapterv1* resolve(Graph* graph,xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    const xrtownershipquiescencev1* quiet[]={xrtNetEngineOwnershipQuiescenceV1Get(),xrtNetStreamOwnershipQuiescenceV1Get(),
        xrtNetListenerOwnershipQuiescenceV1Get(),xrtNetBufPoolOwnershipQuiescenceV1Get(),xrtNetResolverOwnershipQuiescenceV1Get(),
        xrtNetResolveOpOwnershipQuiescenceV1Get(),xrtNetDialOwnershipQuiescenceV1Get(),xrtNetDialFutureOwnershipQuiescenceV1Get(),
        xrtFutureOwnershipQuiescenceV1Get(),xrtCancelOwnershipQuiescenceV1Get(),xrtCancelWatchOwnershipQuiescenceV1Get()};
    for(size_t i=0;i<sizeof(quiet)/sizeof(*quiet);++i)if(ref.Ops==quiet[i]->Ops&&quiet[i]->Busy(ref.Data)){graph->busy=true;return NULL;}
    if(ref.Ops==&stateOps)return &stateAdapter;
    const xnetruntimeownershipv1* runtimes[]={xrtNetStreamRuntimeOwnershipV1Get(),xrtNetListenerRuntimeOwnershipV1Get(),xrtNetDialRuntimeOwnershipV1Get()};
    const xnetresolverlookupownershipv1* lookups[]={&lookupPolicy};
    const xnetresolveownershipv1* requests[]={xrtNetDialResolverPolicyV1Get()};
    const xnetdialownershipv1* callbacks[]={&donePolicy,xrtNetDialFutureDialPolicyV1Get()};
    const xfutureproducerownershipv1* producers[]={xrtNetDialFutureProducerPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[]={xrtNetDialFutureCancelPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[]={xrtNetDialFuturePayloadPolicyV1Get()};
    xfutureproducerownershipv1 producerCopy=*producers[0];xcancelwatchownershipv1 cancelCopy=*cancels[0];
    xnetdialownershipv1 callbackCopy=*callbacks[1];xfuturepayloadownershipv1 payloadCopy=*payloads[0];
    if(graph->wrong==1)producers[0]=&producerCopy;
    if(graph->wrong==2)cancels[0]=&cancelCopy;
    if(graph->wrong==3)callbacks[1]=&callbackCopy;
    if(graph->wrong==4)payloads[0]=&payloadCopy;
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=producers;admission.ProducerPolicyCount=1;
    admission.CancelWatchPolicies=cancels;admission.CancelWatchPolicyCount=1;
    admission.PayloadPolicies=payloads;admission.PayloadPolicyCount=1;
    const xrtownershipadapterv1* adapter=xrtNetEngineOwnershipAdapterV3(ref,NULL,0,NULL,0,NULL,0,runtimes,3,preparation);
    if(!adapter)adapter=xrtNetStreamOwnershipAdapterV1(ref,preparation);
    if(!adapter)adapter=xrtNetListenerOwnershipAdapterV1(ref,preparation);
    if(!adapter)adapter=xrtNetBufPoolOwnershipAdapterV1(ref,preparation);
    if(!adapter)adapter=xrtNetResolverOwnershipAdapterV1(ref,lookups,1,preparation);
    if(!adapter)adapter=xrtNetResolveOpOwnershipAdapterV1(ref,requests,1,preparation);
    if(!adapter)adapter=xrtNetDialOwnershipAdapterV1(ref,callbacks,2,preparation);
    if(!adapter)adapter=xrtNetDialFutureOwnershipAdapterV1(ref,preparation);
    if(!adapter)adapter=xrtFutureOwnershipAdapterV4(ref,&admission,preparation);
    if(!adapter)adapter=xrtCancelOwnershipAdapterV2(ref,cancels,1,preparation);
    if(!adapter)adapter=xrtCancelWatchOwnershipAdapterV1(ref,cancels,1,preparation);
    if(!adapter)adapter=xrtNetAddrListOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtErrorOwnershipAdapterV1(ref);
    return adapter;
}
static bool admit(xrtownershipref ref,ptr raw)
{const xrtownershippreparationv1* preparation=NULL;return resolve(raw,ref,&preparation)!=NULL;}
static xrtownershipsnapshot* snapshot(Graph* graph,xrtownershipscope* scope)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
    for(;;){
        freeze(scope);graph->busy=false;xrtownershipref roots[72],slots[72];size_t r=0,s=0;
        roots[r++]=xrtNetEngineOwnership(graph->engine);
        if(graph->resolver)roots[r++]=xrtNetResolverOwnership(graph->resolver);
        roots[r++]=xrtNetListenerOwnership(graph->listener);roots[r++]=(xrtownershipref){graph->state,&stateOps};
        if(graph->dial)roots[r++]=xrtNetDialOwnership(graph->dial);
        if(graph->future)roots[r++]=xrtFutureOwnership(graph->future);
        if(graph->server)roots[r++]=xrtNetStreamOwnership(graph->server);
        for(size_t i=0;i<r;++i)slots[s++]=roots[i];
        if(graph->pinned)for(size_t i=0;i<graph->count;++i){roots[r++]=graph->nodes[i].ref;slots[s++]=graph->nodes[i].ref;}
        xrtownershipsnapshot* result=NULL;
        if(xrtOwnershipSnapshotCreate(roots,r,slots,s,admit,graph,&result))return result;
        CHECK(graph->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(scope));
        CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
    }
}
static void discover(Graph* graph,xrtownershipsnapshot* proof)
{
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){
        xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item)&&!item.Reachable);
        size_t j=0;while(j<graph->count&&(graph->nodes[j].ref.Data!=item.Reference.Data||graph->nodes[j].ref.Ops!=item.Reference.Ops))++j;
        if(j<graph->count)continue;
        CHECK(graph->count<64);Node* node=&graph->nodes[graph->count++];node->ref=item.Reference;
        node->preparation=NULL;node->adapter=resolve(graph,node->ref,&node->preparation);CHECK(node->adapter);
        CHECK(node->adapter->Hold(node->ref.Data)&&node->adapter->Claim(node->ref.Data,graph));
        if(node->ref.Ops==xrtNetDialFutureOwnershipQuiescenceV1Get()->Ops)++frames;
        if(node->ref.Ops==xrtNetDialOwnershipQuiescenceV1Get()->Ops&&xrtNetDialState(node->ref.Data)==XNET_DIAL_CONNECTING)++candidates;
    }
    graph->pinned=true;
}
static void collect(Graph* graph,unsigned order)
{
    xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(graph,&scope);
    if(graph->future){
        bool frame=false,activeDial=false;
        for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode item;CHECK(xrtOwnershipSnapshotNode(proof,i,&item));
            if(item.Reference.Ops==xrtNetDialFutureOwnershipQuiescenceV1Get()->Ops)frame=true;
            if(item.Reference.Ops==xrtNetDialOwnershipQuiescenceV1Get()->Ops&&xrtNetDialState(item.Reference.Data)<XNET_DIAL_CONNECTED)activeDial=true;
        }
        xrtownershipref roots[6];size_t count=0;
        roots[count++]=xrtNetEngineOwnership(graph->engine);
        if(graph->resolver)roots[count++]=xrtNetResolverOwnership(graph->resolver);
        roots[count++]=xrtNetListenerOwnership(graph->listener);roots[count++]=(xrtownershipref){graph->state,&stateOps};
        roots[count++]=xrtFutureOwnership(graph->future);
        for(unsigned wrong=1;wrong<=4;++wrong){
            if((wrong<=2&&!frame)||(wrong==3&&!activeDial)||(wrong==4&&xrtFutureState(graph->future)!=XFUTURE_RESOLVED))continue;
            graph->wrong=wrong;xrtownershipsnapshot* rejected=NULL;
            CHECK(!xrtOwnershipSnapshotCreate(roots,count,roots,count,admit,graph,&rejected)&&!rejected);
            graph->wrong=0;xrtClearError();++refusals;
        }
    }
    discover(graph,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
    for(;;){bool all=true;
        for(size_t step=0;step<graph->count;++step){size_t i=order&1?graph->count-1-step:step;Node* node=&graph->nodes[i];
            if(!node->preparation||node->ready)continue;
            proof=snapshot(graph,&scope);discover(graph,proof);xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
            xrtownershipprepareresult result=node->preparation->Prepare(node->ref.Data,graph);CHECK(result!=XRT_OWNERSHIP_PREPARE_FAILED);
            node->ready=result==XRT_OWNERSHIP_PREPARE_READY;all=all&&node->ready;
        }
        if(all)break;
        CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
    }
    proof=snapshot(graph,&scope);discover(graph,proof);xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<graph->count;++i){Node* node=&graph->nodes[i];CHECK(!node->preparation||node->preparation->Ready(node->ref.Data));node->adapter->Clear(node->ref.Data,graph);}
    CHECK(xrtOwnershipScopeEnd(&scope));
    for(size_t step=0;step<graph->count;++step){Node* node=&graph->nodes[order&2?graph->count-1-step:step];
        CHECK(!node->adapter->Finish||node->adapter->Finish(node->ref.Data,graph));}
    CHECK(xrtNetEngineTryDestroy(graph->engine)==XNET_RETIRE_READY);
    if(graph->resolver)CHECK(xrtNetResolverTryDestroy(graph->resolver)==XNET_RETIRE_READY);
    xrtNetListenerDestroy(graph->listener);xrtNetDialDestroy(graph->dial);xrtFutureDestroy(graph->future);
    xrtNetStreamDestroy(graph->server);stateDrop(graph->state);
    for(size_t step=0;step<graph->count;++step){Node* node=&graph->nodes[order&4?graph->count-1-step:step];node->adapter->Drop(node->ref.Data);}
    ++graphs;
}
static void owned(xnetportbackend backend,unsigned order,bool cancel)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,!cancel,true);
    xnetaddr address;CHECK(xrtNetListenerLocal(graph.listener,&address));xnetdialconfig config;xrtNetDialConfigInit(&config);
    config.Stream.ReadMode=XNET_STREAM_READ_PROBE;config.Timeout=0;
    CHECK(stateHold(graph.state));graph.dial=xrtNetDialOwnedV1(graph.engine,graph.resolver,"owned.invalid",address.Port,&config,NULL,NULL,graph.state,&donePolicy);CHECK(graph.dial);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
    if(cancel){
        while(!xrtAtomic32Load(&graph.state->entered,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        xrtownershipscope scope={0};const xrtownershippreparationv1* preparation=NULL;const xrtownershipadapterv1* adapter=NULL;
        xrtownershipref ref=xrtNetDialOwnership(graph.dial);const xnetdialownershipv1* allowed[]={&donePolicy};
        for(;;){freeze(&scope);if(!xrtNetDialOwnershipQuiescenceV1Get()->Busy(graph.dial))break;
            CHECK(xrtOwnershipScopeEnd(&scope));CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        size_t references;CHECK(ref.Ops->Count(ref.Data,&references)&&references==3);
        const xrtownershippreparationv1* marker=(const xrtownershippreparationv1*)(uintptr_t)1;preparation=marker;
        CHECK(!xrtNetDialOwnershipAdapterV1(ref,NULL,0,&preparation)&&preparation==marker);++refusals;
        xnetdialownershipv1 copy=donePolicy;const xnetdialownershipv1* wrong[]={&copy};
        CHECK(!xrtNetDialOwnershipAdapterV1(ref,wrong,1,&preparation)&&preparation==marker);++refusals;
        adapter=xrtNetDialOwnershipAdapterV1(ref,allowed,1,&preparation);CHECK(adapter&&adapter->Hold(graph.dial)&&adapter->Claim(graph.dial,&graph));
        CHECK(xrtOwnershipScopeEnd(&scope));
        for(;;){xrtownershipprepareresult result=preparation->Prepare(graph.dial,&graph);CHECK(result!=XRT_OWNERSHIP_PREPARE_FAILED);
            if(result==XRT_OWNERSHIP_PREPARE_READY)break;
            CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        CHECK(xrtAtomic32Load(&graph.state->done,XMEMORY_ACQUIRE)&&graph.state->result==XNET_RESULT_CANCELLED);
        /* READY removes the runtime slot; its final local Drop may still be
         * in flight. Re-prove actual Worker quiescence, then verify all three
         * remaining real references rather than assuming epilogue timing. */
        for(;;){freeze(&scope);if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(graph.engine))break;
            CHECK(xrtOwnershipScopeEnd(&scope));CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        CHECK(ref.Ops->Count(ref.Data,&references)&&references==3); /* user, real pin, still-owned Resolver Data */
        adapter->Restore(graph.dial,&graph);CHECK(xrtOwnershipScopeEnd(&scope));adapter->Drop(graph.dial);
        xrtAtomic32Store(&graph.state->gate,1,XMEMORY_RELEASE);++pending;
    }else{
        while(!xrtAtomic32Load(&graph.state->done,XMEMORY_ACQUIRE)){CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        CHECK(graph.state->result==XNET_RESULT_OK);
        while(!(graph.server=xrtNetListenerAccept(graph.listener))){CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        xnetstream* client=xrtAtomicPtrLoad(&graph.state->client,XMEMORY_ACQUIRE);CHECK(client);
        CHECK(xrtNetStreamSend(client,"a\0b",3)==XNET_RESULT_OK);
        size_t offset=0;while(offset<3){xnetbytes* bytes=xrtNetStreamRecv(graph.server,3-offset,xl6DeadlineAfterMillisecondsV1(5000),NULL);CHECK(bytes);
            xbytesview view=xrtNetBytesView(bytes);CHECK(view.Size&&view.Size<=3-offset&&!memcmp(view.Data,"a\0b"+offset,view.Size));offset+=view.Size;xrtNetBytesDestroy(bytes);}
        ++ordinary;
    }
    collect(&graph,order);balanced(&before);
}
static xnetaddrlist* endpoints(Graph* graph)
{
    xnetaddr addresses[2];CHECK(xrtNetListenerLocal(graph->listener,&addresses[1]));
    CHECK(xrtNetAddrLoopback(&addresses[0],XNET_FAMILY_IPV6,0));
    return xrtNetAddrListCreate(addresses,2); /* the explicitly excluded family is never dialled */
}
static void future(xnetportbackend backend,unsigned order,bool addresses,bool complete)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true,!addresses);
    xnetaddr address;CHECK(xrtNetListenerLocal(graph.listener,&address));xnetdialconfig config;xrtNetDialConfigInit(&config);config.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    if(addresses){
        xnetaddrlist* list=endpoints(&graph);CHECK(list);config.Family=XNET_FAMILY_IPV4;
        graph.future=xrtNetDialAddressesAsync(graph.engine,list,&config,NULL,NULL);xrtNetAddrListDestroy(list);
    }else graph.future=xrtNetDialAsync(graph.engine,graph.resolver,"future.invalid",address.Port,&config,NULL,NULL);
    CHECK(graph.future);
    if(complete)CHECK(xrtFutureWaitFor(graph.future,UINT64_C(5000000))==XWAIT_OK&&xrtFutureState(graph.future)==XFUTURE_RESOLVED);
    collect(&graph,order);balanced(&before);
}
static void construction(xnetportbackend backend)
{
    for(unsigned addresses=0;addresses<2;++addresses)for(unsigned budget=0;budget<48;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);Graph graph={0};create(&graph,backend,true,!addresses);
        xnetaddr address;CHECK(xrtNetListenerLocal(graph.listener,&address));
        xnetaddrlist* list=addresses?endpoints(&graph):NULL;CHECK(!addresses||list);
        xnetdialconfig config;xrtNetDialConfigInit(&config);config.Family=XNET_FAMILY_IPV4;config.Stream.ReadMode=XNET_STREAM_READ_PROBE;
        xrtownershipscope scope={0};XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(10000);
        /* Park the actual Engine before injecting caller-thread constructor
         * failures. No native counts, queues or lifecycle slots are forged. */
        for(;;){freeze(&scope);if(!xrtNetEngineOwnershipQuiescenceV1Get()->Busy(graph.engine))break;
            CHECK(xrtOwnershipScopeEnd(&scope));CHECK(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        CHECK(xrtMemDebugFailAfter(budget));++budgets;
        graph.future=addresses?xrtNetDialAddressesAsync(graph.engine,list,&config,NULL,NULL):
            xrtNetDialAsync(graph.engine,graph.resolver,"budget.invalid",address.Port,&config,NULL,NULL);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!graph.future){CHECK(triggered);++allocationFailures;}else CHECK(!triggered);
        xrtClearError();CHECK(xrtOwnershipScopeEnd(&scope));xrtNetAddrListDestroy(list);
        collect(&graph,budget%8);balanced(&before);
    }
}
int main(int argc,char** argv)
{
    CHECK(xrtMemDebugEnable(true));xnetportbackend backend=XNET_PORT_SELECT;
    if(argc>1&&!strcmp(argv[1],"native")){
        #ifdef _WIN32
        backend=XNET_PORT_IOCP;
        #else
        backend=XNET_PORT_EPOLL;
        #endif
    }
    for(unsigned order=0;order<8;++order){owned(backend,order,false);owned(backend,order,true);
        future(backend,order,false,false);future(backend,order,true,false);future(backend,order,true,true);}
    construction(backend);
    CHECK(xrtMemDebugReset());printf("[PASS] Dial graph cases=%u ordinary=%u pending-resolver=%u refusals=%u observed-frames=%u observed-connecting=%u budgets=%u failures=%u; balanced backend=%d\n",graphs,ordinary,pending,refusals,frames,candidates,budgets,allocationFailures,(int)backend);return 0;
}
