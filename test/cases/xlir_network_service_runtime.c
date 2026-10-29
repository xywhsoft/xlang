#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_network_service.h"
#include <assert.h>
#include <stdio.h>
static const XL6_NetworkServiceRuntimeV2* api;
typedef struct TraceProbe { size_t edges; bool inspect; } TraceProbe;
static bool trace_edge(xrtownershipref reference,void* data)
{
    TraceProbe* probe=data;size_t count;
    if(reference.Data)++probe->edges;
    return !probe->inspect || (reference.Data && reference.Ops && reference.Ops->Count &&
        reference.Ops->Trace && reference.Ops->Count(reference.Data,&count));
}
static bool trace_services(const XL6_NetworkServices* services,bool inspect,size_t* edges)
{
    TraceProbe probe={0};probe.inspect=inspect;xrtownershipscope freeze={0};
    if(!xrtOwnershipFreezeTryBegin(&freeze))return false;
    bool result=api->Trace(services,trace_edge,&probe);
    assert(xrtOwnershipScopeEnd(&freeze));if(edges)*edges=probe.edges;return result;
}
static bool wait_readable(const XL6_NetworkServices* services,size_t expectedEdges)
{
    xdeadline end=xrtDeadlineAfter(5000000);
    for(;;){
        size_t edges=0;
        if(trace_services(services,true,&edges)&&edges==expectedEdges)return true;
        if(xrtDeadlineExpired(end))return false;
        xrtThreadYield();
    }
}
typedef struct State {
    XL6_NetworkServices services;
    xatomic32 entered,release,done,tailEntered,tailRelease,tailDone,posts;
    xthreadkey* key;
    xnetpost post;
} State;
static void wait_for(xatomic32* flag)
{
    xdeadline end=xrtDeadlineAfter(5000000);
    while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(end));xrtThreadYield();}
}
static void finish(XL6_NetworkServices* s)
{
    xdeadline end=xrtDeadlineAfter(5000000);
    for(;;){
        XL6_ModuleRetireResult result=api->Retire(s,false);
        assert(result!=XL6_MODULE_RETIRE_FAILED);
        if(result==XL6_MODULE_RETIRE_READY)break;
        assert(!api->IsEmpty(s) && !xrtDeadlineExpired(end));xrtThreadYield();
    }
    assert(api->IsEmpty(s) && !s->engine && !s->resolver);
    assert(api->Retire(s,false)==XL6_MODULE_RETIRE_READY);
}
static void rejected_init(XL6_NetworkServices* s)
{
    xnetengine* engine=s->engine;xnetresolver* resolver=s->resolver;
    assert(!api->Initialize(s));xrtClearError();
    assert(s->engine==engine && s->resolver==resolver && s->phase==XL6_NETWORK_SERVICE_RETIRING);
}
static void pinned(void)
{
    XL6_NetworkServices s={0};assert(api->Initialize(&s));
    assert(wait_readable(&s,2));
    xnetengine* engine=s.engine;assert(xrtNetEnginePin(engine));
    xdeadline end=xrtDeadlineAfter(5000000);
    while(s.resolver){assert(api->Retire(&s,false)==XL6_MODULE_RETIRE_BUSY);assert(!xrtDeadlineExpired(end));xrtThreadYield();}
    assert(s.engine==engine);rejected_init(&s);
    assert(wait_readable(&s,1));
    xrtSetErrorKind(XERR_STATE);const xerror* error=xrtGetError();assert(error);
    assert(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<64;++i){
        assert(api->Retire(&s,false)==XL6_MODULE_RETIRE_BUSY && s.engine==engine && !api->IsEmpty(&s));
        assert(xrtGetError()==error && xrtNetEngineState(engine)==XNET_ENGINE_RUNNING);
    }
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xrtNetEngineUnpin(engine));finish(&s);assert(xrtGetError()==error);xrtClearError();
    assert(api->Initialize(&s) && api->Retire(&s,true)==XL6_MODULE_RETIRE_READY && api->IsEmpty(&s));
}
static void tail(void* data)
{
    State* s=data;xrtAtomic32Store(&s->tailEntered,1,XMEMORY_RELEASE);
    wait_for(&s->tailRelease);xrtAtomic32Store(&s->tailDone,1,XMEMORY_RELEASE);
}
static void resolved(xnetresolveop* op,void* data)
{
    State* s=data;assert(xrtNetResolveOpState(op)==XNET_RESOLVE_RESOLVED);
    assert(xrtThreadKeySet(s->key,s));
    xnetengine* engine=s->services.engine;xnetresolver* resolver=s->services.resolver;
    assert(api->Retire(&s->services,false)==XL6_MODULE_RETIRE_BUSY && !xrtGetError());
    assert(s->services.engine==engine && s->services.resolver==resolver && xrtNetEngineState(engine)==XNET_ENGINE_RUNNING);
    xrtAtomic32Store(&s->entered,1,XMEMORY_RELEASE);wait_for(&s->release);
    xrtAtomic32Store(&s->done,1,XMEMORY_RELEASE);
}
static void worker(void)
{
    State s={0};assert(api->Initialize(&s.services));s.key=xrtThreadKeyCreate(tail);assert(s.key);
    xnetresolveop* op=xrtNetResolverResolve(s.services.resolver,"127.0.0.1",XNET_FAMILY_IPV4,resolved,&s);assert(op);
    wait_for(&s.entered);xnetresolver* resolver=s.services.resolver;rejected_init(&s.services);
    /* Another worker may briefly own the mutation gate. Retry admission, not
     * the safety verdict: once a real root is visited, active callback state
     * must still refuse an ownership snapshot. */
    size_t edges=0; xdeadline inspection=xrtDeadlineAfter(5000000);
    for (;;) {
        bool readable=trace_services(&s.services,true,&edges);
        if (edges) { assert(!readable); break; }
        assert(!xrtDeadlineExpired(inspection)); xrtThreadYield();
    }
    for(unsigned i=0;i<64;++i)assert(api->Retire(&s.services,false)==XL6_MODULE_RETIRE_BUSY && s.services.resolver==resolver);
    xrtAtomic32Store(&s.release,1,XMEMORY_RELEASE);wait_for(&s.tailEntered);
    for(unsigned i=0;i<64;++i)assert(api->Retire(&s.services,false)==XL6_MODULE_RETIRE_BUSY && s.services.resolver==resolver);
    xrtAtomic32Store(&s.tailRelease,1,XMEMORY_RELEASE);finish(&s.services);
    assert(xrtAtomic32Load(&s.tailDone,XMEMORY_ACQUIRE) && xrtAtomic32Load(&s.done,XMEMORY_ACQUIRE));
    xnetaddrlist* addresses=xrtNetResolveOpResult(op);assert(addresses && xrtNetAddrListCount(addresses));
    xrtNetAddrListDestroy(addresses);xrtNetResolveOpDestroy(op);assert(xrtThreadKeyDestroy(s.key));
}
static void nonconvergent(xnetworker* worker,void* data)
{
    State* s=data;
    if(!xrtAtomic32FetchAdd(&s->posts,1,XMEMORY_ACQ_REL)){
        xrtAtomic32Store(&s->entered,1,XMEMORY_RELEASE);wait_for(&s->release);
    }
    if(!xrtNetPost(worker,&s->post,nonconvergent,s)){
        xrtClearError();xrtAtomic32Store(&s->done,1,XMEMORY_RELEASE);
    }
}
static void failed_retirement(void)
{
    State s={0};assert(api->Initialize(&s.services));assert(xrtNetPostInit(&s.post));
    xnetengine* engine=s.services.engine;
    assert(xrtNetPost(xrtNetEngineWorker(engine,0),&s.post,nonconvergent,&s));wait_for(&s.entered);
    xdeadline end=xrtDeadlineAfter(5000000);
    while(s.services.resolver){assert(api->Retire(&s.services,false)==XL6_MODULE_RETIRE_BUSY);assert(!xrtDeadlineExpired(end));xrtThreadYield();}
    assert(api->Retire(&s.services,false)==XL6_MODULE_RETIRE_BUSY);
    xrtAtomic32Store(&s.release,1,XMEMORY_RELEASE);
    XL6_ModuleRetireResult result;
    do{result=api->Retire(&s.services,false);assert(!xrtDeadlineExpired(end));xrtThreadYield();}while(result==XL6_MODULE_RETIRE_BUSY);
    assert(result==XL6_MODULE_RETIRE_FAILED && s.services.engine==engine && !api->IsEmpty(&s.services));
    const xerror* error=xrtGetError();assert(error && xrtErrorCode(error)==XNET_ERROR_ENGINE_STOP);
    assert(api->Retire(&s.services,false)==XL6_MODULE_RETIRE_READY && api->IsEmpty(&s.services) && xrtGetError()==error);
    xrtClearError();assert(xrtAtomic32Load(&s.done,XMEMORY_ACQUIRE));
}
static unsigned partial_failures(void)
{
    unsigned owned=0,failures=0;
    for(unsigned point=0;point<64;++point){
        XL6_NetworkServices s={0};assert(xrtMemDebugFailAfter(point));bool ok=api->Initialize(&s);
        bool injected=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!ok){
            assert(injected && s.phase==XL6_NETWORK_SERVICE_RETIRING);++failures;
            if(s.engine || s.resolver)++owned;
            xrtClearError();rejected_init(&s);
        }
        finish(&s);xrtClearError();
        if(owned>=4)break;
    }
    assert(owned>=4 && failures>=4);return failures;
}
int main(void)
{
    xmemdebugsnapshot before,after;assert(xrtMemDebugEnable(true));
    const XL6_NetworkServiceRuntimeV1* v1=xl6NetworkServiceRuntimeV1Get();
    api=xl6NetworkServiceRuntimeV2Get();assert(v1&&api&&v1->Initialize==api->Initialize&&
        v1->Retire==api->Retire&&v1->IsEmpty==api->IsEmpty&&api->Trace);
    XL6_NetworkServices invalid={0};invalid.phase=XL6_NETWORK_SERVICE_STARTING;
    assert(!trace_services(&invalid,false,NULL));
    xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<12;++i){pinned();worker();failed_retirement();}
    unsigned partial=partial_failures();xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    printf("Network service runtime: 12 pins, 12 worker self-retirements, 12 real TLS tails, 12 shutdown failures and retries, 2304 retained BUSY steps, %u injected initialization failures including 4 partial owners; exact diagnostic and memory balance\n",partial);
    return 0;
}
