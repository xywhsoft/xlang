/* Unmodified real generated programs, using the new typed entry. The live
 * service graph must still refuse: successful retirement is not trace proof. */
#define XRT_MODULE_MEMORY_DEBUG
#define main xlir_network_generated_main
#include XLIR_NETWORK_GENERATED_C
#undef main
#include <assert.h>
#include <windows.h>
static unsigned steps;
static bool edge(xrtownershipref reference,void* context)
{ (void)reference;(void)context;return true; }
static bool trace(void)
{
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    bool result=xl_script_module_ownership_trace(edge,NULL);assert(xrtOwnershipScopeEnd(&freeze));return result;
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
    assert(!xl6_net_engine && !xl6_net_resolver && xlir_module_initialized==0 && trace());
    assert(step()==XL6_MODULE_RETIRE_READY);
}
static void reinit_refused(void)
{
    xnetengine* engine=xl6_net_engine;xnetresolver* resolver=xl6_net_resolver;
    assert(xlir_module_initialized==0 && !trace());
    assert(!xl_script_module_init(NULL) && xrtGetError());xrtClearError();
    assert(xl6_net_engine==engine && xl6_net_resolver==resolver && xlir_module_initialized==0 && !trace());
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
    while(!xlir_continuation_quiesce()){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(xl6_net_engine && xl6_net_resolver && !trace());
    if(!strcmp(mode,"pinned-engine")){
        xnetengine* engine=xl6_net_engine;assert(xrtNetEnginePin(engine));
        while(xl6_net_resolver){assert(step()==XL6_MODULE_RETIRE_BUSY);assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
        reinit_refused();
        for(unsigned n=0;n<16;++n)assert(step()==XL6_MODULE_RETIRE_BUSY && xl6_net_engine==engine);
        assert(xrtNetEngineUnpin(engine));finish();
    }else if(!strcmp(mode,"resolver-worker")){
        Worker state={0};state.engine=xl6_net_engine;state.resolver=xl6_net_resolver;
        state.entered=CreateEventA(NULL,TRUE,FALSE,NULL);state.release=CreateEventA(NULL,TRUE,FALSE,NULL);assert(state.entered && state.release);
        xnetresolveop* operation=xrtNetResolverResolve(state.resolver,"127.0.0.1",XNET_FAMILY_IPV4,on_worker,&state);assert(operation);
        assert(WaitForSingleObject(state.entered,10000)==WAIT_OBJECT_0 && state.retained);reinit_refused();
        for(unsigned n=0;n<16;++n)assert(step()==XL6_MODULE_RETIRE_BUSY && xl6_net_resolver==state.resolver);
        assert(SetEvent(state.release));finish();xrtNetResolveOpDestroy(operation);
        assert(CloseHandle(state.entered) && CloseHandle(state.release));
    }else finish();
    assert(xl_script_module_init(NULL));
    while(!xlir_continuation_quiesce()){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    finish();
    xrtClearError();assert(xrtRuntimeRetireThreadStorage());xrtMemDebugSnapshot(&after);
    bool balanced=before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount && before.OverflowCount==after.OverflowCount &&
        before.UnderflowCount==after.UnderflowCount;
    assert(balanced);
    printf("{\"Mode\":\"%s\",\"Steps\":%u,\"Complete\":true,\"LiveServiceGraphAccepted\":false,\"Balanced\":true,\"ReinitializedAfterReady\":true}\n",mode,steps);
    return 0;
}
