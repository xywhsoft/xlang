/* Tests the real resident bridge with code from an actually unmapped DLL.
 * This is not a substitute for language ALL/ANY deadlock or module-cycle tests. */
#include "release/include/xruntime_decl.h"
#include "src/api/xl_future_bridge.h"
#include "src/runtime/xl6_future_aggregate.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "xlir_future_aggregate_code_owner_box.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
typedef HMODULE AggregateLibrary;
typedef FARPROC AggregateSymbol;
static AggregateLibrary libraryOpen(const char* path) { return LoadLibraryA(path); }
static AggregateSymbol librarySymbol(AggregateLibrary library,const char* name) { return GetProcAddress(library,name); }
static void libraryCloseUnmapped(AggregateLibrary library,void* address)
{
    MEMORY_BASIC_INFORMATION memory;
    assert(FreeLibrary(library));
    assert(VirtualQuery(address,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);
}
#else
#include <dlfcn.h>
#include <pthread.h>
#include <time.h>
typedef void* AggregateLibrary;
typedef void* AggregateSymbol;
static AggregateLibrary libraryOpen(const char* path) { return dlopen(path,RTLD_NOW|RTLD_LOCAL); }
static AggregateSymbol librarySymbol(AggregateLibrary library,const char* name) { return dlsym(library,name); }
static void libraryCloseUnmapped(AggregateLibrary library,void* address)
{
    Dl_info info;
    assert(dlclose(library)==0 && dladdr(address,&info)==0);
}
#endif

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount &&
        after.UseAfterFreeCount == before->UseAfterFreeCount);
}
typedef struct Owner {
    XL6_CodeOwnerGraph descriptor; unsigned pins, acquired, released, unmapped;
    bool closed; AggregateLibrary library; void* address; AggregateBoxHooks hooks;
    XL6_FutureAggregateBoxV1 parent, child;
    xfuture *childSource, *childOutput; xpromise* childPromise;
} Owner;
static int acquire(void* data)
{ Owner* owner = data; if (owner->closed) return 0; ++owner->pins; ++owner->acquired; return 1; }
static void release(void* data)
{
    Owner* owner = data;
    assert(owner->pins && !xl6CodeOwnerScopeActive(&owner->descriptor.base));
    --owner->pins; ++owner->released;
    if (!owner->pins && owner->library) {
        assert(owner->closed && owner->hooks.calls == 3 && owner->hooks.children == 2 && owner->hooks.returned == 5);
        libraryCloseUnmapped(owner->library,owner->address);
        ++owner->unmapped;
    }
}
static bool count(const void* data, size_t* result)
{ const Owner* owner = data; if (xl6CodeOwnerScopeActive(&owner->descriptor.base)) return false; *result = owner->pins; return *result != 0; }
static bool trace(const void* data, xrtownershipvisitor visit, void* context)
{ (void)data; (void)context; return visit != NULL; }
static const xrtownershipops ownerOps = {count, trace};
static xvalue* box_scalar(const void* input) { return xrtValueInt(*(const int64_t*)input); }
static xvalue* box_int(int64_t value) { return xrtValueInt(value); }
#ifdef _WIN32
static DWORD WINAPI unrelated_thread(void* data)
#else
static void* unrelated_thread(void* data)
#endif
{
    Owner* owner = data; xfuture* inputs[] = {owner->childSource};
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(&owner->descriptor.base)); /* active on the other thread */
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(!xl6FutureAggregateAllV2(inputs, 1, owner->child, &owner->descriptor.base) && xrtGetError());
    xrtClearError(); return 0;
}
static void during(void* data, unsigned child)
{
    Owner* owner = data; xrtownershipscope freeze = {0}; size_t refs = 0;
    assert(owner->closed && owner->pins == 1 && !owner->released);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(&owner->descriptor.base) && !count(owner, &refs));
    assert(xrtOwnershipScopeEnd(&freeze));
    if (!child && !owner->childPromise) {
        owner->childPromise = xrtPromiseCreate(&owner->childSource, NULL); assert(owner->childPromise);
#ifdef _WIN32
        HANDLE thread = CreateThread(NULL, 0, unrelated_thread, owner, 0, NULL); assert(thread);
        assert(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0 && CloseHandle(thread));
#else
        pthread_t thread; struct timespec limit;
        assert(pthread_create(&thread,NULL,unrelated_thread,owner)==0);
        assert(clock_gettime(CLOCK_REALTIME,&limit)==0); limit.tv_sec+=10;
        assert(pthread_timedjoin_np(thread,NULL,&limit)==0);
#endif
        xfuture* inputs[] = {owner->childSource, owner->childSource};
        owner->childOutput = xl6FutureAggregateAllV2(inputs, 2, owner->child, &owner->descriptor.base);
        assert(owner->childOutput && !xrtFutureDone(owner->childOutput) && owner->acquired == 1);
    }
}
static void publish(xpromise* promise, int64_t value, AggregateBoxHooks* hooks)
{
    AggregateBoxPayload* payload = xrtMalloc(sizeof(*payload)); assert(payload);
    *payload = (AggregateBoxPayload){value, hooks};
    assert(xl6PayloadResolveOwned(promise, payload, XL6_FUTURE_PAYLOAD_VALUE));
}
static void check_result(xfuture* future, size_t n, int64_t value)
{
    assert(xrtFutureState(future) == XFUTURE_RESOLVED); xvalue* array = *(xvalue**)xrtFutureValue(future);
    assert(xrtValueCount(array) == n);
    for (size_t i = 0; i < n; ++i) { int64_t actual; assert(xrtValueGetInt(xrtValueArrayGet(array, i), &actual) && actual == value); }
}
static void code_graph(Owner*, xfuture*);
static void dll_round(const char* path)
{
    Owner owner = {0}; xmemdebugsnapshot before; xfuture *source = NULL, *output; xpromise* promise;
    xrtMemDebugSnapshot(&before); owner.library = libraryOpen(path); assert(owner.library);
    AggregateSymbol proc = librarySymbol(owner.library,"aggregate_owner_box");
    _Static_assert(sizeof(proc)==sizeof(owner.parent),"Host symbol and callback pointer representations must match");
    memcpy(&owner.parent,&proc,sizeof(proc)); owner.address=(void*)proc;
    proc=librarySymbol(owner.library,"aggregate_owner_child_box"); memcpy(&owner.child,&proc,sizeof(proc)); assert(owner.parent && owner.child);
    owner.descriptor = (XL6_CodeOwnerGraph){{sizeof(owner.descriptor), &owner, acquire, release}, 0x584C36434F574E31ULL, &ownerOps};
    owner.hooks = (AggregateBoxHooks){box_int, during, &owner, 0, 0, 0};
    promise = xrtPromiseCreate(&source, NULL); assert(promise); xfuture* inputs[] = {source, source, source};
    output = xl6FutureAggregateAllV2(inputs, 3, owner.parent, &owner.descriptor.base); assert(output && owner.pins == 1 && owner.acquired == 1);
    code_graph(&owner, output);
    owner.closed = true;
    assert(!xl6FutureAggregateAllV2(inputs, 3, owner.parent, &owner.descriptor.base) && xrtGetError()); xrtClearError();
    publish(promise, 42, &owner.hooks); check_result(output, 3, 42);
    assert(owner.hooks.calls == 3 && owner.hooks.returned == 3 && !owner.released && owner.pins == 1);
    code_graph(&owner, owner.childOutput);
    publish(owner.childPromise, 84, &owner.hooks); check_result(owner.childOutput, 2, 84);
    assert(owner.unmapped == 1 && !owner.pins && owner.acquired == 1 && owner.released == 1);
    xrtPromiseDestroy(owner.childPromise); xrtFutureDestroy(owner.childSource); xrtFutureDestroy(owner.childOutput);
    xrtPromiseDestroy(promise); xrtFutureDestroy(source); xrtFutureDestroy(output); balanced(&before);
}

typedef struct Graph { xrtownershipref refs[64]; size_t count; } Graph;
static bool collect(xrtownershipref ref, void* data)
{
    Graph* graph = data; if (!ref.Data) return true;
    for (size_t i = 0; i < graph->count; ++i) if (ref.Data == graph->refs[i].Data && ref.Ops == graph->refs[i].Ops) return true;
    assert(ref.Ops && graph->count < 64); graph->refs[graph->count++] = ref;
    return ref.Ops->Trace(ref.Data, collect, graph);
}
static bool direct_edge(xrtownershipref ref, void* data)
{ Graph* edges = data; assert(ref.Data && edges->count < 64); edges->refs[edges->count++] = ref; return true; }
static void code_graph(Owner* owner, xfuture* future)
{
    xrtownershipscope freeze = {0}; Graph graph = {0}, mapperEdges = {0}, leaseEdges = {0};
    xrtownershipref mapper = {0}, lease = {0}; unsigned maps = 0, leases = 0, owners = 0; size_t actual;
    assert(xrtOwnershipFreezeTryBegin(&freeze)); assert(collect(xrtFutureOwnership(future), &graph));
    for (size_t i = 0; i < graph.count; ++i) {
        xrtownershipref ref = graph.refs[i];
        if (xl6FutureAggregateOwnershipAdapterV1(ref)) { mapper = ref; ++maps; }
        if (xl6CodeLeaseOwnershipAdapterV1(ref)) { lease = ref; ++leases; }
        if (ref.Data == owner && ref.Ops == &ownerOps) ++owners;
    }
    assert(maps == 1 && leases == 1 && owners == 1 && owner->pins == 1);
    assert(mapper.Ops->Count(mapper.Data, &actual) && actual == 1);
    assert(lease.Ops->Count(lease.Data, &actual) && actual == 1);
    assert(mapper.Ops->Trace(mapper.Data, direct_edge, &mapperEdges) && mapperEdges.count == 1);
    assert(mapperEdges.refs[0].Data == lease.Data && mapperEdges.refs[0].Ops == lease.Ops);
    assert(lease.Ops->Trace(lease.Data, direct_edge, &leaseEdges) && leaseEdges.count == 1);
    assert(leaseEdges.refs[0].Data == owner && leaseEdges.refs[0].Ops == &ownerOps);
    assert(xrtOwnershipScopeEnd(&freeze));
}
static bool resolve(xrtownershipref ref, void* context, const xrtownershipadapterv1** adapter, const xrtownershippreparationv1** preparation)
{
    (void)context; *preparation = NULL;
    const xfuturecombineownershipv1* maps[] = {xl6FutureAggregatePolicyV1Get()};
    const xcancelwatchownershipv1* cancel[] = {xrtFutureCombineCancelPolicyV1Get()};
    const xfuturewatchownershipv2* projected[] = {xrtFutureCombineWatchPolicyV2Get()};
    const xfutureproducerownershipv1* producers[] = {xrtFutureCombineProducerPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[] = {xl6PayloadPolicyGet(XL6_FUTURE_PAYLOAD_VALUE), xl6PayloadPolicyGet(XL6_FUTURE_PAYLOAD_XVALUE)};
    const xfutureownershipadmissionv1 table = {sizeof(table), payloads, 2, producers, 1, NULL, 0, projected, 1, cancel, 1};
    if ((*adapter = xl6FutureAggregateOwnershipAdapterV1(ref))) return true;
    if ((*adapter = xrtFutureCombineOwnershipAdapterV1(ref, maps, 1, preparation))) return true;
    if ((*adapter = xrtCancelWatchOwnershipAdapterV1(ref, cancel, 1, preparation))) return true;
    if ((*adapter = xrtCancelOwnershipAdapterV2(ref, cancel, 1, preparation))) return true;
    if ((*adapter = xrtFutureOwnershipAdapterV4(ref, &table, preparation))) return true;
    if ((*adapter = xl6PayloadOwnershipAdapterV1(ref))) return true;
    if ((*adapter = xrtValueOwnershipAdapterV1(ref))) return true;
    return false;
}
static void graph_round(unsigned mode)
{
    xmemdebugsnapshot before; xfuture *source = NULL, *output; xrtownershipscope freeze = {0};
    xrtMemDebugSnapshot(&before); xpromise* promise = xrtPromiseCreate(&source, NULL); assert(promise);
    xfuture* inputs[] = {source, source, source};
    output = mode == 0 ? xl6FutureAggregateAllV2(inputs, 3, box_scalar, NULL) : mode == 1 ? xl6FutureAggregateRaceV2(inputs, 3) : xl6FutureAggregateWhenAnyIndexV2(inputs, 3); assert(output);
    assert(xrtOwnershipFreezeTryBegin(&freeze)); Graph graph = {0}; assert(collect(xrtFutureOwnership(output), &graph));
    unsigned maps = 0, groups = 0, watches = 0;
    for (size_t i = 0; i < graph.count; ++i) {
        const xrtownershipadapterv1* adapter = NULL; const xrtownershippreparationv1* prep = NULL;
        assert(resolve(graph.refs[i], NULL, &adapter, &prep) && adapter);
        maps += xl6FutureAggregateOwnershipAdapterV1(graph.refs[i]) != NULL;
        const xfuturecombineownershipv1* policies[] = {xl6FutureAggregatePolicyV1Get()};
        groups += xrtFutureCombineOwnershipAdapterV1(graph.refs[i], policies, 1, &prep) != NULL;
        const xcancelwatchownershipv1* cp[] = {xrtFutureCombineCancelPolicyV1Get()};
        watches += xrtCancelWatchOwnershipAdapterV1(graph.refs[i], cp, 1, &prep) != NULL;
    }
    assert(maps == 1 && groups == 1 && watches == 1);
    assert(xrtOwnershipScopeEnd(&freeze));
    XL6_OwnershipPlan* plan = NULL;
    assert(xl6OwnershipPlanTryCreateV2(xrtFutureOwnership(output), resolve, NULL, &plan) == XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryDestroy(plan) == XL6_OWNERSHIP_PLAN_READY && !xrtFutureDone(output));
    assert(xrtPromiseResolve(promise, NULL)); assert(xrtFutureDone(output));
    xrtFutureDestroy(output); xrtPromiseDestroy(promise); xrtFutureDestroy(source); xrtClearError(); balanced(&before);
}
static void legacy_refusal(void)
{
    xmemdebugsnapshot before; xfuture *source = NULL, *output; xrtownershipscope freeze = {0};
    xrtMemDebugSnapshot(&before); xpromise* promise = xrtPromiseCreate(&source, NULL); assert(promise);
    output = xl6FutureAllXValues(&source, 1, box_scalar); assert(output);
    assert(xrtOwnershipFreezeTryBegin(&freeze)); Graph graph = {0}; assert(collect(xrtFutureOwnership(output), &graph));
    size_t refused = 0;
    for (size_t i = 0; i < graph.count; ++i) {
        const xrtownershipadapterv1* adapter = NULL; const xrtownershippreparationv1* prep = NULL;
        if (!resolve(graph.refs[i], NULL, &adapter, &prep)) ++refused;
        assert(!xl6FutureAggregateOwnershipAdapterV1(graph.refs[i]));
    }
    assert(refused >= 2 && xrtOwnershipScopeEnd(&freeze));
    assert(xrtFutureCancel(output)); xrtFutureDestroy(output); xrtPromiseDestroy(promise); xrtFutureDestroy(source); balanced(&before);
}
static unsigned allocation_failures(void)
{
    unsigned failures = 0, accepted = 0, completed = 0;
    for (unsigned arrayForm = 0; arrayForm < 2; ++arrayForm) for (unsigned ready = 0; ready < 2; ++ready) {
        unsigned localFailures = 0, localAccepted = 0;
        for (unsigned point = 0; point < 32; ++point) {
            Owner owner = {0}; xmemdebugsnapshot before; xfuture *source = NULL, *output;
            xrtMemDebugSnapshot(&before); owner.pins = 1; /* actual caller-held entry pin */
            owner.descriptor = (XL6_CodeOwnerGraph){{sizeof(owner.descriptor), &owner, acquire, release}, 0x584C36434F574E31ULL, &ownerOps};
            xpromise* promise = xrtPromiseCreate(&source, NULL); assert(promise);
            xfuture* inputs[] = {source, source}; xvalue* array = xrtValueArray(); assert(array);
            assert(xrtValueArrayAppendNew(array, xrtValueFuture(source)) && xrtValueArrayAppendNew(array, xrtValueFuture(source)));
            if (ready) publish(promise, 42, NULL);
            assert(xrtMemDebugFailAfter(point));
            output = arrayForm ? xl6FutureAggregateAllArrayV2(array, box_scalar, &owner.descriptor.base) : xl6FutureAggregateAllV2(inputs, 2, box_scalar, &owner.descriptor.base);
            bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xrtClearError();
            if (!output) {
                ++failures; ++localFailures; assert(triggered && owner.pins == 1 && owner.acquired == owner.released);
                assert(xrtFutureDone(source) == (bool)ready);
                xcancel* cancel = xrtFutureCancelToken(source); assert(cancel && !xrtCancelRequested(cancel)); xrtCancelDestroy(cancel);
            } else {
                ++accepted; ++localAccepted;
                if (!ready) { assert(owner.pins == 2); assert(xrtFutureCancel(output)); }
                else if (xrtFutureState(output) == XFUTURE_RESOLVED) { check_result(output, 2, 42); ++completed; }
                else assert(triggered && xrtFutureState(output) == XFUTURE_FAILED);
                xrtFutureDestroy(output); assert(owner.pins == 1 && owner.acquired == owner.released);
            }
            xrtValueRelease(array); xrtPromiseDestroy(promise); xrtFutureDestroy(source);
            owner.closed = true; release(&owner); assert(!owner.pins); xrtClearError(); balanced(&before);
        }
        assert(localFailures && localAccepted);
    }
    assert(failures && accepted && completed);
    printf("Aggregate owned mapper OOM: 128 budgets, %u actual preparation failures, %u accepted, %u complete results\n", failures, accepted, completed);
    return failures;
}
int main(int argc, char** argv)
{
    assert(argc == 2 && xrtMemDebugEnable(true));
    xrtSetErrorKind(XERR_STATE); xrtClearError(); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned round = 0; round < 100; ++round) { dll_round(argv[1]); for (unsigned mode = 0; mode < 3; ++mode) graph_round(mode); legacy_refusal(); }
    assert(allocation_failures());
    balanced(&before);
    puts("Aggregate mapper code graph: 200 exact context-to-lease-to-owner chains, one physical acquisition and real reference counts");
    puts("Aggregate code ownership: 100 real DLL parent/child lifetimes after Close, 100 cross-thread admission refusals, 500 scoped box calls, 300 fully admitted physical graphs, 100 legacy policy refusals; exact memory and no synthetic code roots"); return 0;
}
