/* Core-only actual Server graph: no Future/async-accept API or implementation.
 * Offline metadata and ordinary self-created literal loopback listeners. */
#include "net-server-native-modules.h"
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
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"[FAIL] line %u: %s\n",(unsigned)__LINE__,#c);exit(1);}}while(0)
enum {MAX_NODES=32};
typedef struct Node {
    xrtownershipref ref;
    const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* prepare;
    const XL6_OwnershipFinishV1* completion;
    bool ready;
} Node;
typedef struct Scene {XL6_OwnershipCatalog* catalog;bool busy;} Scene;
static void freeze(xrtownershipscope* scope)
{
    XL6_DeadlineV1 due=xl6DeadlineAfterMillisecondsV1(10000);
    while(!xrtOwnershipFreezeTryBegin(scope)){CHECK(!xl6DeadlineExpiredV1(due));xrtThreadYield();}
}
static bool admit(xrtownershipref ref,ptr data)
{
    Scene* scene=data;const xrtownershipadapterv1* adapter=NULL;
    const xrtownershippreparationv1* prepare=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    XL6_OwnershipAdmissionV1 result=xl6OwnershipCatalogProbeV1(ref,scene->catalog,&adapter,&prepare,&completion);
    if(result==XL6_OWNERSHIP_ADMISSION_BUSY)scene->busy=true;
    return result==XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
static xrtownershipsnapshot* snapshot(Scene* scene,xrtownershipscope* scope,const xrtownershipref* roots,size_t count)
{
    XL6_DeadlineV1 due=xl6DeadlineAfterMillisecondsV1(10000);
    for(;;){freeze(scope);scene->busy=false;xrtownershipsnapshot* proof=NULL;
        if(xrtOwnershipSnapshotCreate(roots,count,roots,count,admit,scene,&proof))return proof;
        CHECK(scene->busy);xrtClearError();CHECK(xrtOwnershipScopeEnd(scope));CHECK(!xl6DeadlineExpiredV1(due));xrtThreadYield();}
}
static void exercise(XL6_OwnershipCatalog* catalog,xnetportbackend backend)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Backend=backend;ec.Workers=2;ec.NodeCacheBytes=0;
    xnetengine* engine=xrtNetEngineCreate(&ec);CHECK(engine&&xrtNetEngineStart(engine));
    xnetserverconfig config;xrtNetServerConfigInit(&config);xnetlistenconfig second=config.Listen;
    CHECK(xrtNetAddrLoopback(&config.Listen.Address,XNET_FAMILY_IPV4,0));
    CHECK(xrtNetAddrLoopback(&second.Address,XNET_FAMILY_IPV4,0));
    config.Additional=&second;config.AdditionalCount=1;
    xnetserver* server=xrtNetServerStart(engine,&config,NULL,NULL,NULL);CHECK(server);
    CHECK(xrtNetServerEndpointCount(server)==2&&xrtNetServerListenerCount(server)==2);
    Scene scene={catalog,false};Node nodes[MAX_NODES];size_t count=0;
    xrtownershipref roots[MAX_NODES+2];roots[0]=xrtNetEngineOwnership(engine);roots[1]=xrtNetServerOwnership(server);
    xrtownershipscope scope={0};xrtownershipsnapshot* proof=snapshot(&scene,&scope,roots,2);
    CHECK(xrtOwnershipSnapshotNodeCount(proof)==8);
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode found;CHECK(xrtOwnershipSnapshotNode(proof,i,&found));
        CHECK(!found.Reachable&&count<MAX_NODES);Node* node=&nodes[count++];memset(node,0,sizeof(*node));node->ref=found.Reference;
        CHECK(xl6OwnershipCatalogProbeV1(node->ref,catalog,&node->adapter,&node->prepare,&node->completion)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
        CHECK(node->adapter->Hold(node->ref.Data)&&node->adapter->Claim(node->ref.Data,&scene));roots[count+1]=node->ref;}
    xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));
    XL6_DeadlineV1 due=xl6DeadlineAfterMillisecondsV1(10000);
    for(;;){bool ready=true;
        for(size_t i=0;i<count;++i){Node* node=&nodes[(i+1)%count];if(!node->prepare||node->ready)continue;
            xrtownershipprepareresult result=node->prepare->Prepare(node->ref.Data,&scene);
            CHECK(result!=XRT_OWNERSHIP_PREPARE_FAILED);node->ready=result==XRT_OWNERSHIP_PREPARE_READY;ready=ready&&node->ready;}
        if(ready)break;
        CHECK(!xl6DeadlineExpiredV1(due));xrtThreadYield();}
    proof=snapshot(&scene,&scope,roots,count+2);
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(proof);++i){xrtownershipnode found;
        CHECK(xrtOwnershipSnapshotNode(proof,i,&found)&&!found.Reachable);}
    xrtOwnershipSnapshotDestroy(proof);
    for(size_t i=0;i<count;++i){Node* node=&nodes[i];CHECK(!node->prepare||node->prepare->Ready(node->ref.Data));node->adapter->Clear(node->ref.Data,&scene);}
    CHECK(xrtOwnershipScopeEnd(&scope));
    for(size_t i=0;i<count;++i){Node* node=&nodes[i];
        if(node->completion)CHECK(node->completion->Step(node->ref.Data,&scene)==XRT_OWNERSHIP_FINISH_READY);
        else CHECK(!node->adapter->Finish||node->adapter->Finish(node->ref.Data,&scene));}
    CHECK(xrtNetEngineTryDestroy(engine)==XNET_RETIRE_READY);xrtNetServerDestroy(server);
    for(size_t i=count;i;--i)nodes[i-1].adapter->Drop(nodes[i-1].ref.Data);
    xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount&&before.OverflowCount==after.OverflowCount&&before.UnderflowCount==after.UnderflowCount);
}
int main(int argc,char** argv)
{
    CHECK(argc<=2);xnetportbackend backend=XNET_PORT_SELECT;
    if(argc==2&&!strcmp(argv[1],"native")){
#ifdef _WIN32
        backend=XNET_PORT_IOCP;
#elif defined(__linux__)
        backend=XNET_PORT_EPOLL;
#endif
    }
    CHECK(xrtMemDebugEnable(true));xl6_error_scope errorScope={0};CHECK(xl6ErrorScopeBegin(&errorScope));
    xl6_error_result error=xl6ErrorScopeEnd(&errorScope);CHECK(error.ok);xrtErrorFree(error.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "net-server-native-domains.h"
    };
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));CHECK(catalog);
    for(unsigned i=0;i<3;++i)exercise(catalog,backend);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("[PASS] Server core-only: exact 8-node owned graph, physical retirement, balanced, backend=%d cases=3\n",(int)backend);return 0;
}
