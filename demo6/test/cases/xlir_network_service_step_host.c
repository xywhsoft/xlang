/* Unmodified real generated programs, using the typed retryable entry. Stable
 * service slots are real graph edges; transient native callbacks still make
 * their child ownership views refuse a recursive snapshot. */
#define XRT_MODULE_MEMORY_DEBUG
#define main xlir_network_generated_main
#include XLIR_NETWORK_GENERATED_C
#undef main
#include <assert.h>
#include <windows.h>
static unsigned steps;
typedef struct TraceProbe { size_t edges; bool inspect; } TraceProbe;
static bool edge(xrtownershipref reference,void* context)
{
    TraceProbe* probe=context;size_t count;
    if(reference.Data)++probe->edges;
    return !probe->inspect || (reference.Data && reference.Ops && reference.Ops->Count &&
        reference.Ops->Trace && reference.Ops->Count(reference.Data,&count));
}
static bool trace(bool inspect,size_t* edges)
{
    TraceProbe probe={0};probe.inspect=inspect;
    xrtownershipscope freeze={0};if(!xrtOwnershipFreezeTryBegin(&freeze))return false;
    bool result=xl_script_module_ownership_trace(edge,&probe);assert(xrtOwnershipScopeEnd(&freeze));
    if(edges)*edges=probe.edges;
    return result;
}
static void wait_trace(size_t expectedEdges)
{
    xdeadline deadline=xrtDeadlineAfter(10000000u);
    for(;;){
        size_t edges=0;
        if(trace(true,&edges)&&edges==expectedEdges)return;
        assert(!xrtDeadlineExpired(deadline));
        xrtThreadYield();
    }
}
static XL6_ModuleRetireResult step(void)
{
    ++steps;XL6_ModuleRetireResult result=xl_script_module_retire_step();
    assert(result==XL6_MODULE_RETIRE_BUSY || result==XL6_MODULE_RETIRE_READY);
    assert(!xrtGetError());return result;
}
static void finish(void)
{
    xdeadline deadline=xrtDeadlineAfter(10000000u);
    while(step()==XL6_MODULE_RETIRE_BUSY){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    size_t edges=99;
    assert(!xl6_net_engine && !xl6_net_resolver && xlir_module_initialized==0 && trace(true,&edges) && edges==0);
    assert(step()==XL6_MODULE_RETIRE_READY);
}
static void reinit_refused(bool graphReadable)
{
    xnetengine* engine=xl6_net_engine;xnetresolver* resolver=xl6_net_resolver;
    assert(xlir_module_initialized==0 && trace(true,NULL)==graphReadable);
    assert(!xl_script_module_init(NULL) && xrtGetError());xrtClearError();
    assert(xl6_net_engine==engine && xl6_net_resolver==resolver && xlir_module_initialized==0 &&
        trace(true,NULL)==graphReadable);
}
typedef struct Worker { HANDLE entered,release; xnetengine* engine; xnetresolver* resolver; bool retained; } Worker;
static void on_worker(xnetresolveop* operation,void* data)
{
    Worker* state=data;assert(operation && xrtNetResolveOpState(operation)!=XNET_RESOLVE_PENDING);
    assert(step()==XL6_MODULE_RETIRE_BUSY);
    state->retained=xl6_net_engine==state->engine && xl6_net_resolver==state->resolver;
    assert(SetEvent(state->entered));assert(WaitForSingleObject(state->release,10000)==WAIT_OBJECT_0);
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));
    const char* mode=argv[1];assert(!strcmp(mode,"idle") || !strcmp(mode,"pinned-engine") || !strcmp(mode,"resolver-worker"));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    assert(xl_script_module_init(NULL));
    xdeadline deadline=xrtDeadlineAfter(10000000u);
    while(!xlir_continuation_quiesce(false)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(xl6_net_engine && xl6_net_resolver);wait_trace(2);
    if(!strcmp(mode,"pinned-engine")){
        xnetengine* engine=xl6_net_engine;assert(xrtNetEnginePin(engine));
        while(xl6_net_resolver){assert(step()==XL6_MODULE_RETIRE_BUSY);assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
        reinit_refused(true);
        for(unsigned n=0;n<16;++n)assert(step()==XL6_MODULE_RETIRE_BUSY && xl6_net_engine==engine);
        assert(xrtNetEngineUnpin(engine));finish();
    }else if(!strcmp(mode,"resolver-worker")){
        Worker state={0};state.engine=xl6_net_engine;state.resolver=xl6_net_resolver;
        state.entered=CreateEventA(NULL,TRUE,FALSE,NULL);state.release=CreateEventA(NULL,TRUE,FALSE,NULL);assert(state.entered && state.release);
        xnetresolveop* operation=xrtNetResolverResolve(state.resolver,"127.0.0.1",XNET_FAMILY_IPV4,on_worker,&state);assert(operation);
        assert(WaitForSingleObject(state.entered,10000)==WAIT_OBJECT_0 && state.retained);reinit_refused(false);
        for(unsigned n=0;n<16;++n)assert(step()==XL6_MODULE_RETIRE_BUSY && xl6_net_resolver==state.resolver);
        assert(SetEvent(state.release));finish();xrtNetResolveOpDestroy(operation);
        assert(CloseHandle(state.entered) && CloseHandle(state.release));
    }else finish();
    assert(xl_script_module_init(NULL));
    while(!xlir_continuation_quiesce(false)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    finish();
    xrtClearError();assert(xrtRuntimeRetireThreadStorage());xrtMemDebugSnapshot(&after);
    bool balanced=before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount && before.OverflowCount==after.OverflowCount &&
        before.UnderflowCount==after.UnderflowCount;
    assert(balanced);
    bool transient_graph_exercised=strcmp(mode,"resolver-worker")==0;
    printf("{\"Mode\":\"%s\",\"Steps\":%u,\"Complete\":true,\"StableServiceGraphAccepted\":true,\"TransientGraphExercised\":%s,\"TransientGraphRefused\":%s,\"Balanced\":true,\"ReinitializedAfterReady\":true}\n",
        mode,steps,transient_graph_exercised?"true":"false",transient_graph_exercised?"true":"false");
    return 0;
}
