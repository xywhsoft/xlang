/* Real saved AOT program, resident host, and its public init/checked-unit
 * entries. No generated source edits or substitute engine/resolver objects.
 * Failed retirement is repaired by the HOST only after recording the lost
 * owner, so discovery never leaves a worker running or hides that failure. */
#define XRT_MODULE_MEMORY_DEBUG
#define main xlir_network_generated_main
#include XLIR_NETWORK_GENERATED_C
#undef main
#include <assert.h>
#include <windows.h>

static bool record_edge(xrtownershipref reference, void* context)
{
    size_t* count=context;
    if(reference.Data) ++*count;
    return true;
}
typedef struct WorkerUnit {
    HANDLE done;
    int result, diagnostic;
    bool resolverRetained;
    xnetresolver* resolver;
} WorkerUnit;
static void unit_on_resolver(xnetresolveop* operation, void* data)
{
    WorkerUnit* call=data;
    assert(operation && xrtNetResolveOpState(operation)!=XNET_RESOLVE_PENDING);
    call->result=xl_script_module_unit_checked();
    call->diagnostic=xrtGetError()!=NULL;
    call->resolverRetained=xl6_net_resolver==call->resolver;
    xrtClearError();
    assert(SetEvent(call->done));
}
static bool memory_equal(const xmemdebugsnapshot* before,const xmemdebugsnapshot* after)
{
    return before->LiveCount==after->LiveCount && before->LiveBytes==after->LiveBytes &&
        after->AllocCount-before->AllocCount==after->FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after->InvalidFreeCount && before->DoubleFreeCount==after->DoubleFreeCount &&
        before->UseAfterFreeCount==after->UseAfterFreeCount && before->OverflowCount==after->OverflowCount &&
        before->UnderflowCount==after->UnderflowCount;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    const char* mode=argv[1];
    assert(!strcmp(mode,"idle") || !strcmp(mode,"pinned-engine") || !strcmp(mode,"resolver-worker"));
    xmemdebugsnapshot before,after;
    assert(xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);
    assert(xl_script_module_init(NULL));
    xdeadline deadline=xrtDeadlineAfter(10000000u);
    while(!xlir_continuation_quiesce(false)) { assert(!xrtDeadlineExpired(deadline));xrtThreadYield(); }
    assert(xlir_module_initialized==2 && xl6_net_engine && xl6_net_resolver && !xrtGetError());
    xnetengine* engine=xl6_net_engine;
    xnetresolver* resolver=xl6_net_resolver;
    xnetenginestats stats;assert(xrtNetEngineStats(engine,&stats));
    size_t edges=0;xrtownershipscope freeze={0};
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    bool traced=xl_script_module_ownership_trace(record_edge,&edges);
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtClearError();
    int first=0,retry=1,diagnostic=0,rescued=0;
    bool retained=true,cleanup=false;
    if(!strcmp(mode,"idle")){
        first=xl_script_module_unit_checked();diagnostic=xrtGetError()!=NULL;
        cleanup=first && !diagnostic && !xl6_net_engine && !xl6_net_resolver;
    }else if(!strcmp(mode,"pinned-engine")){
        assert(xrtNetEnginePin(engine));
        first=xl_script_module_unit_checked();diagnostic=xrtGetError()!=NULL;
        retained=xl6_net_engine==engine;
        assert(xrtNetEngineUnpin(engine));xrtClearError();
        retry=xl_script_module_unit_checked();
        /* A lost Create owner is still alive after failed Destroy. Rescue
         * exactly that saved allocation; do not restore a generated slot. */
        if(!retained){xrtClearError();assert(xrtNetEngineDestroy(engine));rescued=1;}
        cleanup=!first && retained && retry && !xl6_net_engine && !xl6_net_resolver;
    }else{
        WorkerUnit call={0};call.resolver=resolver;call.done=CreateEventA(NULL,TRUE,FALSE,NULL);assert(call.done);
        xnetresolveop* operation=xrtNetResolverResolve(resolver,"127.0.0.1",XNET_FAMILY_IPV4,unit_on_resolver,&call);
        assert(operation && WaitForSingleObject(call.done,15000)==WAIT_OBJECT_0);
        first=call.result;diagnostic=call.diagnostic;retained=call.resolverRetained;
        assert(CloseHandle(call.done));xrtNetResolveOpDestroy(operation);xrtClearError();
        retry=xl_script_module_unit_checked();
        if(!retained){xrtClearError();assert(xrtNetResolverDestroy(resolver));rescued=1;}
        cleanup=!first && retained && retry && !xl6_net_engine && !xl6_net_resolver;
    }
    xrtClearError();assert(xrtRuntimeRetireThreadStorage());xrtMemDebugSnapshot(&after);
    bool balanced=memory_equal(&before,&after);
    bool complete=traced && cleanup && balanced && xlir_module_initialized==0;
    printf("{\"Mode\":\"%s\",\"TraceComplete\":%s,\"VisitedEdges\":%zu,\"NativeLiveBefore\":%u,"
        "\"FirstUnit\":%d,\"Diagnostic\":%d,\"OwnerRetained\":%s,\"RetryUnit\":%d,\"HostRescuedOrphan\":%d,"
        "\"CleanupComplete\":%s,\"BalancedAfterRescue\":%s,\"LiveDelta\":%lld,\"ByteDelta\":%lld,\"Complete\":%s}\n",
        mode,traced?"true":"false",edges,(unsigned)stats.LiveObjects,first,diagnostic,retained?"true":"false",retry,rescued,
        cleanup?"true":"false",balanced?"true":"false",(long long)after.LiveCount-(long long)before.LiveCount,
        (long long)after.LiveBytes-(long long)before.LiveBytes,complete?"true":"false");
    return complete?0:1;
}
