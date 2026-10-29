/* Only offline state/ownership and literal numeric fast paths. No lookup of
 * names, sockets, external DNS/traffic, probes or network-security tests. */
#include "release/include/xl_clock.h"
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_execution_net_resolver.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef TEST_ROOT
#error TEST_ROOT is required
#endif
#if TEST_ROOT!=2&&TEST_ROOT!=4
static void checked(xl6_error_scope* scope,xerrkind expected)
{
    xl6_error_result result=xl6ErrorScopeEnd(scope);assert(!result.ok&&xrtErrorKind(result.error)==expected);
    xrtErrorFree(result.error);assert(!xrtGetError());
}
#endif
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
#if TEST_ROOT==0||TEST_ROOT==5
static void lists(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xnetaddr addresses[3];
    assert(xrtNetAddrLoopback(addresses,XNET_FAMILY_IPV4,10));
    assert(xrtNetAddrLoopback(addresses+1,XNET_FAMILY_IPV6,20));addresses[2]=addresses[0];
    void* list=xrtNetAddrListCreate(addresses,3);assert(list&&xl6NetAddressListCountV1(list)==2);
    void* clone=xl6NetAddressListCloneV1(list);void* mapped=xl6NetAddressListWithPortV1(list,9000);
    assert(clone==list&&mapped&&xl6NetAddressListPortV1(mapped,1)==9000&&xl6NetAddressListPortV1(list,0)==10);
    void* first=xl6NetAddressListAtV1(list,0);assert(first&&xrtNetAddrEqual(first,addresses));xl6NetAddressDestroyV1(first);
    xstrview text=xl6NetAddressListTextV1(list,1);assert(text.Size==3&&!memcmp(text.Data,"::1",3));xrtFree((void*)text.Data);
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));assert(!xl6NetAddressListAtV1(list,-1));checked(&error,XERR_RANGE);
    const XL6_NativeHandlePolicyV1* policy=xl6NetAddressListHandlePolicyV1Get();
    xvalue* boxed=xl6NativeHandleTakeV1(&mapped,policy);assert(boxed&&!mapped);xrtValueRelease(boxed);
    xl6NetAddressListDestroyV1(list);assert(xl6NetAddressListCountV1(clone)==2);xl6NetAddressListDestroyV1(clone);balanced(before);
}
#endif
#if TEST_ROOT==1||TEST_ROOT==5
static void options(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* original=xl6NetResolverConfigCreateV1();assert(original);
    void* changed=xl6NetResolverConfigWithWorkersV1(original,1);assert(changed);
    void* ttl=xl6NetResolverConfigWithSuccessTTLMillisV1(changed,23);assert(ttl);
    assert(xl6NetResolverConfigSuccessTTLMillisV1(ttl)==23&&((xnetresolverconfig*)ttl)->SuccessTTL==23000);
    assert(xl6NetResolverConfigWorkersV1(changed)==1&&xl6NetResolverConfigWorkersV1(original)>0);
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));
    assert(!xl6NetResolverConfigWithWorkersV1(original,0));checked(&error,XERR_RANGE);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6NetResolverConfigWithFailureTTLMillisV1(original,UINT64_MAX));checked(&error,XERR_RANGE);
    xl6NetResolverConfigDestroyV1(ttl);xl6NetResolverConfigDestroyV1(changed);xl6NetResolverConfigDestroyV1(original);balanced(before);
}
#endif
#if TEST_ROOT==2||TEST_ROOT==5
static void statistics(void)
{
    xnetresolverstats value={0};value.Submitted=UINT64_MAX;value.CacheHits=UINT64_C(0x12345678ABCDEF01);
    assert(xl6NetResolverSnapshotSubmittedV1(&value)==UINT64_MAX&&xl6NetResolverSnapshotCacheHitsV1(&value)==value.CacheHits);
    void* heap=xrtMemDup(&value,sizeof(value));assert(heap);xl6NetResolverSnapshotDestroyV1(heap);
}
#endif
#if TEST_ROOT==3||TEST_ROOT==5
static XL6_OwnershipCatalog* catalog;
enum {GRAPH_LIMIT=32};
typedef struct Node {
    xrtownershipref ref;size_t refs;
    const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* prepare;
} Node;
typedef struct Graph {Node nodes[GRAPH_LIMIT];size_t count,edges;} Graph;
static bool edge(xrtownershipref ref,void* data)
{
    Graph* graph=data;if(!ref.Data)return true;
    ++graph->edges;
    for(size_t i=0;i<graph->count;++i)if(graph->nodes[i].ref.Data==ref.Data&&graph->nodes[i].ref.Ops==ref.Ops)return true;
    assert(graph->count<GRAPH_LIMIT);graph->nodes[graph->count++]=(Node){.ref=ref};return true;
}
static bool collect(Graph* graph,XL6_ExecutionSourceV1* source)
{
    *graph=(Graph){0};assert(edge(xl6ExecutionSourceOwnershipV1(source),graph));graph->edges=0;
    for(size_t i=0;i<graph->count;++i){Node* node=&graph->nodes[i];const XL6_OwnershipFinishV1* finish=NULL;
        XL6_OwnershipAdmissionV1 status=xl6OwnershipCatalogProbeV1(node->ref,catalog,&node->adapter,&node->prepare,&finish);
        if(status!=XL6_OWNERSHIP_ADMISSION_ADMITTED)return false;
        assert(node->adapter&&!finish&&node->ref.Ops->Count(node->ref.Data,&node->refs));
        assert(node->ref.Ops->Trace(node->ref.Data,edge,graph));}
    xrtownershipresult actual={0};assert(xrtOwnershipInspect(&graph->nodes[0].ref,1,NULL,0,&actual));
    assert(actual.NodeCount==graph->count&&actual.EdgeCount==graph->edges);return true;
}
static void freeze(xrtownershipscope* scope)
{while(!xrtOwnershipFreezeTryBegin(scope))xrtThreadYield();}
static void destroy_source(XL6_ExecutionSourceV1** source)
{
    assert(xl6ExecutionSourceCloseV1(*source));XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(*source){XL6_ExecutionRetireResultV1 done=xl6ExecutionSourceDestroyV1(source);assert(done.ok);
        if(done.ready)break;
        assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void graph_case(unsigned order,bool restore)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;
    XL6_ExecutionResourceV1* resource=xl6ExecutionNetResolverCreateV1(source,&config);assert(resource);
    Graph graph;xrtownershipscope frozen={0};XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){freeze(&frozen);if(collect(&graph,source))break;
        assert(xrtOwnershipScopeEnd(&frozen)&&!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    /* Native Resolver also owns its prebuilt cancellation error. */
    assert(graph.count==4);int token=0;
    for(size_t i=0;i<graph.count;++i)assert(graph.nodes[i].adapter->Hold(graph.nodes[i].ref.Data)&&graph.nodes[i].adapter->Claim(graph.nodes[i].ref.Data,&token));
    assert(xrtOwnershipScopeEnd(&frozen));bool ready[GRAPH_LIMIT]={0};size_t pending=graph.count;
    while(pending){for(size_t step=0;step<graph.count;++step){size_t i=order&1?graph.count-1-step:step;Node* node=&graph.nodes[i];
        if(ready[i])continue;
        xrtownershipprepareresult result=node->prepare?node->prepare->Prepare(node->ref.Data,&token):XRT_OWNERSHIP_PREPARE_READY;
        assert(result!=XRT_OWNERSHIP_PREPARE_FAILED);if(result==XRT_OWNERSHIP_PREPARE_READY){ready[i]=true;--pending;}}
        assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    freeze(&frozen);
    for(size_t i=0;i<graph.count;++i){Node* node=&graph.nodes[i];assert(!node->prepare||node->prepare->Ready(node->ref.Data));
        if(restore)node->adapter->Restore(node->ref.Data,&token);else node->adapter->Clear(node->ref.Data,&token);}
    assert(xrtOwnershipScopeEnd(&frozen));
    if(!restore){assert(xrtMemDebugFailAfter(0));
        for(unsigned pass=0;pass<2;++pass)for(size_t step=0;step<graph.count;++step){size_t i=order&2?graph.count-1-step:step;
            Node* node=&graph.nodes[i];assert(!node->adapter->Finish||node->adapter->Finish(node->ref.Data,&token));}
        assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();}
    if(restore)for(size_t step=0;step<graph.count;++step){size_t i=order&4?graph.count-1-step:step;graph.nodes[i].adapter->Drop(graph.nodes[i].ref.Data);}
    xl6ExecutionResourceReleaseV1(resource);destroy_source(&source);
    if(!restore)for(size_t step=0;step<graph.count;++step){size_t i=order&4?graph.count-1-step:step;graph.nodes[i].adapter->Drop(graph.nodes[i].ref.Data);}
    assert(!source);balanced(before);
}
static void constructor_prefixes(void)
{
    size_t terminal=SIZE_MAX;
    for(size_t position=0;position<256;++position){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
        xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;
        xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error)&&xrtMemDebugFailAfter(position));
        void* resource=xl6NetResolverCreateV1((XL6_ExecutionServicesV1){source},&config);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result result=xl6ErrorScopeEnd(&error);
        assert((resource!=NULL)==result.ok);xrtErrorFree(result.error);assert(!xrtGetError());
        if(resource)xl6NetResolverDestroyV1(resource);
        destroy_source(&source);balanced(before);
        if(!hit){assert(resource);terminal=position;break;}
    }
    assert(terminal!=SIZE_MAX);printf("Resolver construction prefixes=%zu first-no-hit=%zu balanced\n",terminal+1,terminal);
}
static void lifecycle(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;
    void* resolver=xl6NetResolverCreateV1((XL6_ExecutionServicesV1){source},&config);assert(resolver&&!xl6NetResolverClosedV1(resolver));
    void* snapshot=xl6NetResolverSnapshotV1(resolver);assert(snapshot&&xl6NetResolverSnapshotWorkersV1(snapshot)==1&&xl6NetResolverSnapshotSubmittedV1(snapshot)==0);
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));assert(!xl6NetResolverWaitV1(resolver,0));checked(&error,XERR_STATE);
    xl6NetResolverClearV1(resolver);xl6NetResolverCloseV1(resolver);
    assert(xl6NetResolverClosedV1(resolver)&&xl6NetResolverWaitV1(resolver,5000)&&xl6NetResolverWaitV1(resolver,0));
    XL6_ExecutionResourceSnapshotV1 physical={.size=sizeof(physical)};
    assert(xl6ExecutionResourceSnapshotV1Get(resolver,&physical)&&!physical.nativePresent&&physical.owners==1);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6NetResolverQueryV1(resolver,(xstrview){"127.0.0.1",9},4));checked(&error,XERR_STATE);
    void* copied=xl6NetResolverConfigurationV1(resolver);assert(copied&&xl6NetResolverConfigWorkersV1(copied)==1);xl6NetResolverConfigDestroyV1(copied);
    xl6NetResolverDestroyV1(resolver);xl6NetResolverSnapshotDestroyV1(snapshot);assert(xl6ExecutionSourceCloseV1(source));
    XL6_DeadlineV1 limit=xl6DeadlineAfterMillisecondsV1(5000);
    while(source){XL6_ExecutionRetireResultV1 done=xl6ExecutionSourceDestroyV1(&source);assert(done.ok);if(done.ready)break;assert(!xl6DeadlineExpiredV1(limit));xrtThreadYield();}
    assert(!source);balanced(before);
}
#endif
#if TEST_ROOT==4||TEST_ROOT==5
static void operation(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;
    xnetresolver* resolver=xrtNetResolverCreate(&config);assert(resolver);
    xfuture* source=xrtNetResolveAsync(resolver,"127.0.0.1",XNET_FAMILY_IPV4);assert(source);
    xfuture* signal=xl6NetResolveOperationWaitV1(source);assert(signal&&xrtFutureWaitFor(signal,UINT64_C(5000000))==XWAIT_OK);
    xfutureresult result={0};assert(xrtFutureResult(signal,&result)&&result.State==XFUTURE_RESOLVED&&result.Value&&*(const bool*)result.Value);
    assert(xl6NetResolveOperationDoneV1(source));
    xrtFutureDestroy(signal);xl6NetResolveOperationDestroyV1(source);
    assert(xrtNetResolverDestroy(resolver));balanced(before);
}
static void wait_prefixes(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xnetresolverconfig config;xrtNetResolverConfigInit(&config);config.Workers=1;
    xnetresolver* resolver=xrtNetResolverCreate(&config);assert(resolver);
    xfuture* source=xrtNetResolveAsync(resolver,"::1",XNET_FAMILY_IPV6);assert(source&&xrtFutureWaitFor(source,UINT64_C(5000000))==XWAIT_OK);
    /* Outcome readiness is not physical callback/thread retirement. Join the
     * real producer before taking per-prefix snapshots; the retained Future
     * and its immutable list remain valid independently of the service. */
    assert(xrtNetResolverDestroy(resolver));resolver=NULL;
    size_t terminal=SIZE_MAX;
    for(size_t position=0;position<128;++position){
        xmemdebugsnapshot iteration;xrtMemDebugSnapshot(&iteration);xl6_error_scope error={0};
        assert(xl6ErrorScopeBegin(&error)&&xrtMemDebugFailAfter(position));
        xfuture* signal=xl6NetResolveOperationWaitV1(source);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result result=xl6ErrorScopeEnd(&error);assert((signal!=NULL)==result.ok);xrtErrorFree(result.error);assert(!xrtGetError());
        if(signal){xfutureresult value={0};assert(xrtFutureResult(signal,&value));
            if(value.State==XFUTURE_RESOLVED)assert(value.Value&&*(const bool*)value.Value);
            else assert(hit&&value.State==XFUTURE_FAILED&&xrtErrorKind(value.Error)==XERR_MEMORY);
            xrtFutureDestroy(signal);}
        balanced(iteration);if(!hit){assert(signal);terminal=position;break;}
    }
    assert(terminal!=SIZE_MAX);xl6NetResolveOperationDestroyV1(source);balanced(before);
    printf("Resolver wait prefixes=%zu first-no-hit=%zu balanced\n",terminal+1,terminal);
}
#endif
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
#if TEST_ROOT==0||TEST_ROOT==5
    lists();
#endif
#if TEST_ROOT==1||TEST_ROOT==5
    options();
#endif
#if TEST_ROOT==2||TEST_ROOT==5
    statistics();
#endif
#if TEST_ROOT==3||TEST_ROOT==5
    const XL6_OwnershipDomainV1* domains[]={
#if TEST_ROOT==3
#include "resolver.domains.h"
#else
#include "all.domains.h"
#endif
    };
    catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));assert(catalog);
    for(unsigned order=0;order<8;++order){graph_case(order,false);graph_case(order,true);}
    xl6OwnershipCatalogDestroyV1(catalog);catalog=NULL;
    constructor_prefixes();
    lifecycle();
#endif
#if TEST_ROOT==4||TEST_ROOT==5
    operation();
    wait_prefixes();
#endif
    assert(!xrtGetError());balanced(before);printf("Resolver native root %d: offline/numeric lifecycle balanced\n",TEST_ROOT);return 0;
}
