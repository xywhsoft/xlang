#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "lib/xruntime/xruntime.h"
#include "xlir_resident_closure_dll.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

static const XL6_ClosureRuntimeV1* api;
typedef struct Owner {
    XL6_CodeOwnerGraph descriptor;
    XL6_Closure* closure;
    xvalue* value;
    bool closed;
    unsigned pins, acquired, released, unmapped;
    HMODULE library;
    void* address;
    ClosureTestHooks hooks;
    unsigned expectedInvoked, expectedDropped, expectedAdapted;
} Owner;
typedef struct Fixture {
    Owner a, b;
    XL6_Closure* child;
    ClosureTestInvoke invoke;
    ClosureTestDrop drop;
    XL6_CodeContextTrace trace;
    XL6_ClosureBoxEntry adapter;
    const char* const* names;
    const xlir_call_default* defaults;
    bool createChild;
} Fixture;
static int acquire(void* data)
{
    Owner* owner = (Owner*)data;
    if (owner->closed) return 0;
    ++owner->acquired; ++owner->pins; return 1;
}
static void release(void* data)
{
    Owner* owner = (Owner*)data;
    assert(owner->pins); --owner->pins; ++owner->released;
    if (!owner->pins && owner->closed && owner->library) {
        MEMORY_BASIC_INFORMATION memory;
        assert(owner->hooks.invoked == owner->expectedInvoked && owner->hooks.dropped == owner->expectedDropped &&
            owner->hooks.adapted == owner->expectedAdapted);
        assert(FreeLibrary(owner->library)); owner->library = NULL;
        assert(VirtualQuery(owner->address, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_FREE);
        ++owner->unmapped;
    }
}
static bool count(const void* data, size_t* result)
{ const Owner* owner = (const Owner*)data; *result = owner->pins + !owner->closed; return true; }
static bool trace_owner(const void* data, xrtownershipvisitor visit, void* context)
{
    const Owner* owner = (const Owner*)data;
    return (!owner->closure || visit(api->Ownership(owner->closure), context)) &&
        (!owner->value || visit(xrtValueOwnership(owner->value), context));
}
static const xrtownershipops ops = {count, trace_owner};
static bool trace_environment(const ClosureTestEnvironment* env, XL6_ModuleOwnershipVisit visit, void* context)
{ return visit(xrtValueOwnership(env->captured), context); }
static void drop_environment(ClosureTestEnvironment* env)
{ xrtValueRelease(env->captured); xrtFree(env); }
static bool result_value(xrtcallresult* result, int64_t number)
{
    xvalue* value = xrtValueInt(number); bool ok;
    if (!value) return false;
    ok = xrtCallResultPushTake(result, &value); xrtValueRelease(value); return ok;
}
static ClosureTestEnvironment* environment(Fixture* fixture)
{
    ClosureTestEnvironment* env = xrtMalloc(sizeof(*env)); assert(env);
    *env = (ClosureTestEnvironment){&fixture->a.hooks, &fixture->b.hooks, fixture, xrtValueInt(10042)};
    assert(env->captured); return env;
}
static XL6_Closure* make(Fixture* fixture, ClosureTestEnvironment* env)
{
    return api->Create(&fixture->a.descriptor.base, (void (*)(void))fixture->invoke, NULL, env,
        fixture->drop, fixture->names, fixture->defaults, 1, fixture->trace);
}
static void during(ClosureTestEnvironment* env)
{
    Fixture* fixture = (Fixture*)env->context;
    if (fixture->createChild) {
        assert(fixture->a.closed && !fixture->child && fixture->a.pins == 1);
        fixture->child = make(fixture, environment(fixture)); assert(fixture->child);
        assert(fixture->a.acquired == 1); /* Inherit the accepted A scope, not a new admission. */
    }
}
static bool resident_invoke(void* data, int64_t* output)
{ (void)data; *output = 42; return true; }
static void resident_drop(void* data)
{ ClosureTestEnvironment* env = data; ++env->producer->dropped; drop_environment(env); }
static bool resident_trace(const void* data, XL6_ModuleOwnershipVisit visit, void* context)
{ return trace_environment(data, visit, context); }
static bool resident_adapter(void* data, const xrtcallframe* frame, xrtcallresult* result)
{ (void)data; (void)frame; return result_value(result, 42); }
static void prepare(Fixture* fixture)
{
    *fixture = (Fixture){0};
    Owner* owners[] = {&fixture->a, &fixture->b};
    for (size_t i = 0; i < 2; ++i) {
        owners[i]->descriptor = (XL6_CodeOwnerGraph){{sizeof(XL6_CodeOwnerGraph), owners[i], acquire, release},
            0x584C36434F574E31ULL, &ops};
        owners[i]->hooks = (ClosureTestHooks){during, drop_environment, trace_environment, result_value, 0, 0, 0};
    }
    fixture->invoke = resident_invoke; fixture->drop = resident_drop;
    fixture->trace = resident_trace; fixture->adapter = resident_adapter;
}
static void load_symbol(HMODULE library, const char* name, void* output, size_t size)
{ FARPROC symbol = GetProcAddress(library, name); assert(symbol && size == sizeof(symbol)); memcpy(output, &symbol, size); }
static void libraries(Fixture* fixture, char** paths)
{
    ClosureTestNames names; ClosureTestDefaults defaults;
    fixture->a.library = LoadLibraryA(paths[0]); fixture->b.library = LoadLibraryA(paths[1]);
    assert(fixture->a.library && fixture->b.library && fixture->a.library != fixture->b.library);
    load_symbol(fixture->a.library, "closure_generated_invoke", &fixture->invoke, sizeof(fixture->invoke));
    load_symbol(fixture->a.library, "closure_generated_drop", &fixture->drop, sizeof(fixture->drop));
    load_symbol(fixture->a.library, "closure_generated_trace", &fixture->trace, sizeof(fixture->trace));
    load_symbol(fixture->a.library, "closure_generated_names", &names, sizeof(names));
    load_symbol(fixture->a.library, "closure_generated_defaults", &defaults, sizeof(defaults));
    load_symbol(fixture->b.library, "closure_generated_adapter", &fixture->adapter, sizeof(fixture->adapter));
    fixture->a.address = (void*)fixture->invoke; fixture->b.address = (void*)fixture->adapter;
    fixture->names = names(); fixture->defaults = defaults();
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static void graph(Fixture* fixture, size_t roots, size_t reachable)
{
    const xrtownershipref anchors[] = {{&fixture->a, &ops}, {&fixture->b, &ops}};
    xrtownershipresult result;
    assert(xrtOwnershipInspect(anchors, 2, NULL, 0, &result));
    assert(result.NodeCount == 8 && result.EdgeCount == 9 && result.ExternalRootCount == roots && result.ReachableAnchorCount == reachable);
}
static void dll_lifetimes(char** paths)
{
    for (unsigned round = 0; round < 50; ++round) {
        Fixture fixture; XL6_Closure* closure; xvalue* boxed; int64 answer;
        xrtcallresult result = XRT_CALL_RESULT_INIT; xmemdebugsnapshot before;
        xrtMemDebugSnapshot(&before); prepare(&fixture); libraries(&fixture, paths);
        closure = make(&fixture, environment(&fixture)); assert(closure);
        boxed = api->Box(&fixture.b.descriptor.base, closure, fixture.adapter, 12345); assert(boxed);
        api->Release(closure); fixture.createChild = true;
        fixture.a.expectedInvoked = fixture.b.expectedAdapted = 1; fixture.a.expectedDropped = 2;
        fixture.a.closed = fixture.b.closed = true;
        assert(!strcmp(fixture.names[0], "amount") && fixture.defaults[0].i64_value == 42);
        assert(xrtValueInvoke(boxed, NULL, &result));
        assert(xrtCallResultCount(&result) == 1 && xrtValueGetInt(xrtCallResultGet(&result, 0), &answer) && answer == 42);
        xrtCallResultUnit(&result); assert(fixture.child && !fixture.a.unmapped && !fixture.b.unmapped);
        xrtValueRelease(boxed);
        assert(fixture.b.unmapped == 1 && !fixture.a.unmapped && fixture.a.hooks.dropped == 1);
        assert(!strcmp(fixture.child->call_parameter_names[0], "amount") && fixture.child->call_parameter_defaults[0].i64_value == 42);
        api->Release(fixture.child);
        assert(fixture.a.unmapped == 1 && fixture.a.acquired == 1 && fixture.b.acquired == 1 &&
            fixture.a.released == 1 && fixture.b.released == 1 && !xrtGetError());
        balanced(&before);
    }
    for (unsigned round = 0; round < 25; ++round) {
        Fixture fixture; XL6_Closure** payload; xvalue* alias; XL6_Closure* closure; xmemdebugsnapshot before;
        xrtMemDebugSnapshot(&before); prepare(&fixture); libraries(&fixture, paths);
        fixture.a.closure = make(&fixture, environment(&fixture)); assert(fixture.a.closure);
        fixture.b.value = api->Box(&fixture.b.descriptor.base, fixture.a.closure, fixture.adapter, 12345); assert(fixture.b.value);
        fixture.a.expectedDropped = 1; fixture.a.closed = fixture.b.closed = true;
        graph(&fixture, 0, 0);
        alias = xrtValueRetain(fixture.b.value); graph(&fixture, 1, 2);
        xrtValueRelease(alias); graph(&fixture, 0, 0);
        closure = api->Retain(fixture.a.closure); graph(&fixture, 1, 1);
        api->Release(closure); graph(&fixture, 0, 0);
        alias = fixture.b.value; fixture.b.value = NULL; xrtValueRelease(alias);
        assert(fixture.b.unmapped == 1 && !fixture.a.unmapped);
        payload = xrtMalloc(sizeof(*payload)); assert(payload);
        *payload = fixture.a.closure; fixture.a.closure = NULL; api->PayloadDrop(payload, NULL);
        assert(fixture.a.unmapped == 1 && !xrtGetError()); balanced(&before);
    }
}
static DWORD WINAPI worker(void* data)
{
    XL6_Closure* closure = data;
    for (unsigned i = 0; i < 1000; ++i) api->Release(api->Retain(closure));
    return 0;
}
static void native_references(void)
{
    for (unsigned round = 0; round < 25; ++round) {
        Fixture fixture; XL6_Closure* closure; HANDLE threads[4]; size_t refs; xmemdebugsnapshot before;
        xrtMemDebugSnapshot(&before); prepare(&fixture);
        closure = make(&fixture, environment(&fixture)); assert(closure); fixture.a.closed = true;
        for (unsigned i = 0; i < 4; ++i) { threads[i] = CreateThread(NULL, 0, worker, closure, 0, NULL); assert(threads[i]); }
        assert(WaitForMultipleObjects(4, threads, TRUE, 10000) == WAIT_OBJECT_0);
        for (unsigned i = 0; i < 4; ++i) assert(CloseHandle(threads[i]));
        xrtownershipref view = api->Ownership(closure); assert(view.Ops->Count(view.Data, &refs) && refs == 1);
        XL6_ClosureScope scope = {0}; api->Enter(&scope, closure); api->Release(closure);
        assert(fixture.a.pins == 1 && !fixture.a.hooks.dropped);
        api->Leave(&scope); assert(!fixture.a.pins && fixture.a.hooks.dropped == 1); balanced(&before);
    }
}
static unsigned allocation_failures(void)
{
    unsigned createFailures = 0, boxFailures = 0, boxPositions = 0;
    for (unsigned round = 0; round < 25; ++round) {
        unsigned positions = 0;
        for (unsigned stage = 0; stage < 2; ++stage) for (unsigned fail = 0; fail < 32; ++fail) {
            Fixture fixture; ClosureTestEnvironment* env; XL6_Closure* closure = NULL; xvalue* boxed = NULL;
            xmemdebugsnapshot before; bool triggered, success; xerror* primary;
            xrtMemDebugSnapshot(&before); prepare(&fixture); env = environment(&fixture);
            primary = xrtErrorCreate(XERR_STATE, "test", 7, "primary"); assert(primary);
            if (stage) { closure = make(&fixture, env); assert(closure); }
            if (round & 1) xrtSetErrorTake(xrtErrorRef(primary));
            assert(xrtMemDebugFailAfter(fail));
            if (stage) boxed = api->Box(&fixture.b.descriptor.base, closure, fixture.adapter, 12345);
            else closure = make(&fixture, env);
            triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            success = stage ? boxed != NULL : closure != NULL;
            assert(success == !triggered && fixture.a.hooks.dropped == 0);
            if (round & 1) assert(xrtGetError() == primary);
            else if (!success) assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            else assert(!xrtGetError());
            xrtClearError(); xrtErrorFree(primary);
            if (stage) { if (!success) { ++boxFailures; ++positions; } xrtValueRelease(boxed); api->Release(closure); }
            else if (!success) { ++createFailures; drop_environment(env); }
            else api->Release(closure);
            assert(!fixture.a.pins && !fixture.b.pins && fixture.a.acquired == fixture.a.released &&
                fixture.b.acquired == fixture.b.released); balanced(&before);
            if (success) break;
            assert(fail != 31);
        }
        if (!round) boxPositions = positions;
        assert(positions == boxPositions && positions >= 4);
    }
    assert(createFailures == 50 && boxFailures == 25 * boxPositions);
    printf("Resident closure OOM: 50 create refusals, %u box refusals across %u allocation positions, 50 successes, borrowed inputs and primary diagnostics preserved\n", boxFailures, boxPositions);
    return boxFailures;
}
int main(int argc, char** argv)
{
    assert(argc == 3 && xrtMemDebugEnable(true)); api = xl6ClosureRuntimeV1Get(); assert(api);
    dll_lifetimes(argv + 1); native_references(); (void)allocation_failures();
    puts("Resident closures: 50 distinct A/B DLL invocation lifetimes, 25 dual-owner cyclic graph transitions, 100000 native retain/release pairs, callback-return-before-unmap and allocator balance");
    assert(xrtMemDebugReset()); return 0;
}
