/* Two real TCC modules share source/schema, never physical code ownership.
 * Module A binds a receiver constructed by B. Only resident callable scopes
 * protect the saved generated dispatcher after A's export returns. */
#include "xl.h"
#include "src/api/xl_module_state.h"
#include "src/runtime/xl6_closure.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
/* Also link the B163 baseline: this resident function already existed there,
 * but its compiler-private header did not yet declare the entry. */
extern const XL6_OwnershipReadBoundaryV1* xlModuleOwnershipReadBoundaryV1(void);
#ifndef XL6_BOUND_ACTIVITY_SOURCE
#define XL6_BOUND_ACTIVITY_SOURCE "test/cases/xlir_bound_activity.xl"
#endif

static XL_Module *adapterModule, *receiverModule;
static xvalue* callable;
static xvalue* receiver;
static void (*makeReader)(void);
static xatomic64 ready = {0}, proceed = {0};
static bool parking, checkRead;
static unsigned probes, drops, readChecks;
static void* toolAddress;
static size_t toolSize;
static bool captureTool;

static bool live(const xmemdebugallocation* allocation, void* ignored)
{
    (void)ignored;
    if (captureTool) { assert(!toolAddress); toolAddress = allocation->Address; toolSize = allocation->Size; }
    else assert(allocation->Address == toolAddress && allocation->Size == toolSize);
    return true;
}
static bool countEdge(xrtownershipref ref, void* context)
{ if (ref.Data) ++*(size_t*)context; return true; }
static void inspect(bool active, bool foreign)
{
    const XL6_OwnershipReadBoundaryV1* boundary = xlModuleOwnershipReadBoundaryV1();
    XL6_OwnershipReadScopeV1 read = {0}; xrtownershipscope freeze = {0};
    if (checkRead && !adapterModule->destroyRequested)
        assert(boundary->Begin(&read) == XL6_OWNERSHIP_READ_READY_V1);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    const XL6_CodeOwner* owner = &adapterModule->codeOwnerGraph.base;
    assert(adapterModule->activeCalls == 0);
    assert(xl6CodeOwnerScopeActive(owner) == active);
    assert(xl6CodeOwnerScopeForeign(owner) == foreign);
    if (read.self) {
        xrtownershipref module = {adapterModule, adapterModule->codeOwnerGraph.ops};
        assert(boundary->Admits(&read, module) == !foreign);
        if (!foreign) {
            size_t count = 0, edges = 0;
            assert(module.Ops->Count(module.Data, &count));
            assert(count == 1u + adapterModule->activeCalls + adapterModule->codeLeases +
                adapterModule->preparationRefs + adapterModule->snapshotRefs);
            assert(module.Ops->Trace(module.Data, countEdge, &edges) && edges == 0);
        } else { size_t count; assert(!module.Ops->Count(module.Data, &count)); }
        ++readChecks;
    }
    assert(xrtOwnershipScopeEnd(&freeze)); if (read.self) boundary->End(&read);
}
void bound_activity_probe(int64_t phase)
{
    assert(phase == 1 || phase == 2);
    if (phase == 1) {
        ++probes; inspect(true, false);
        if (parking) {
            xrtAtomic64Store(&ready, 1, XMEMORY_RELEASE);
            while (!xrtAtomic64Load(&proceed, XMEMORY_ACQUIRE)) xrtThreadYield();
        }
    } else {
        xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(xl6CodeOwnerScopeActive(&receiverModule->codeOwnerGraph.base));
        assert(!xl6CodeOwnerScopeForeign(&receiverModule->codeOwnerGraph.base));
        assert(xrtOwnershipScopeEnd(&freeze)); ++drops;
    }
}
void bound_activity_save_receiver(xvalue* value)
{ assert(!receiver); receiver = xrtValueRetain(value); }
void bound_activity_save_reader(xvalue* value)
{ assert(!callable); callable = xrtValueRetain(value); }
xvalue* bound_activity_load_receiver(void)
{ assert(receiver); return xrtValueRetain(receiver); }
static xvalue* createReader(void)
{
    xl6_foreign_error_scope scope = {0}; assert(!callable);
    if (!xl6ForeignCallBegin(&scope)) return NULL;
    makeReader(); xl6_foreign_error_result outcome = xl6ForeignCallEnd(&scope);
    if (!outcome.ok) {
        assert(!callable); xrtSetErrorTake(outcome.error); return NULL;
    }
    assert(!outcome.error && callable); xvalue* result = callable; callable = NULL; return result;
}
static bool invoke(void)
{
    xrtcallframe frame = {0}; xrtcallresult result = {0}; int64_t value = 0;
    bool ok = xrtValueInvoke(callable, &frame, &result);
    if (ok) assert(xrtCallResultCount(&result) == 1 &&
        xrtValueGetInt(xrtCallResultGet(&result, 0), &value) && value == 42);
    else assert(xrtCallResultCount(&result) == 0);
    xrtCallResultUnit(&result); return ok;
}
static int worker(void* ignored)
{ (void)ignored; assert(invoke()); return 0; }
#if defined(XL6_BOUND_ACTIVITY_ERASED_TEST)
static bool erasedEntry(void* closure, const xrtcallframe* frame, xrtcallresult* result)
{ (void)closure; (void)frame; (void)result; return true; }
static void erasedInvoke(void) {}
static void erasedTransport(void)
{
    XL6_Closure* closure = xl6ClosureRuntimeV2Get()->Create(NULL, erasedInvoke, NULL, NULL, NULL, NULL, NULL, 0);
    assert(closure);
    assert(!xl6ClosureRuntimeV1Get()->Box(NULL, closure, erasedEntry, 0) && xrtGetError());
    xrtClearError(); xvalue* value = xl6ClosureBoxErasedV1(NULL, closure, erasedEntry); assert(value && !xrtGetError());
    xl6ClosureRuntimeV1Get()->Release(closure); xrtValueRelease(value);
}
#endif
int main(int argc, char** argv)
{
    assert(argc == 6); checkRead = argv[5][0] == '1';
    XL_ContextOptions options = xlDefaultContextOptions(); options.workDir = ".";
    options.tccDllPath = argv[1]; options.tccLibPath = argv[2]; options.tempDir = argv[3];
    xl6ProductMemoryBegin(SIZE_MAX);
    XL_Context* warm = xlCreateContext(&options); assert(warm && xlAddIncludePath(warm, argv[4]));
    XL_Module* tool = xlCompileCSourceStringToModule(warm, "int bound_tool_probe(void) { return 42; }\n"); assert(tool);
    int (*warmProbe)(void) = (int (*)(void))xlModuleGetSymbol(tool, "bound_tool_probe"); assert(warmProbe() == 42);
    xlDestroyModule(tool); xlDestroyContext(warm); xlRuntimeUnit(); xrtClearError();
    xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline); assert(baseline.LiveCount == 1);
    captureTool = true; assert(xrtMemDebugVisitLive(live, NULL)); captureTool = false;
#if defined(XL6_BOUND_ACTIVITY_ERASED_TEST)
    erasedTransport();
#endif
    XL_Context* context = xlCreateContext(&options); assert(context && xlAddIncludePath(context, argv[4]));
    assert(xlRegisterCFunction(context, "bound_activity_probe", (void*)bound_activity_probe));
    assert(xlRegisterCFunction(context, "bound_activity_save_receiver", (void*)bound_activity_save_receiver));
    assert(xlRegisterCFunction(context, "bound_activity_save_reader", (void*)bound_activity_save_reader));
    assert(xlRegisterCFunction(context, "bound_activity_load_receiver", (void*)bound_activity_load_receiver));
    receiverModule = xlCompileScriptFileToModule(context, XL6_BOUND_ACTIVITY_SOURCE);
    if (!receiverModule) fprintf(stderr, "%s\n", xlGetLastError(context));
    assert(receiverModule);
    adapterModule = xlCompileScriptFileToModule(context, XL6_BOUND_ACTIVITY_SOURCE);
    if (!adapterModule) fprintf(stderr, "%s\n", xlGetLastError(context));
    assert(adapterModule && adapterModule != receiverModule);
    if (checkRead) assert(adapterModule->ownershipReadContract && receiverModule->ownershipReadContract);
    void (*makeReceiver)(void) = (void (*)(void))xlModuleGetSymbol(receiverModule, "bound_activity_receiver");
    makeReader = (void (*)(void))xlModuleGetSymbol(adapterModule, "bound_activity_reader");
    assert(makeReceiver && makeReader);
    makeReceiver(); assert(receiver && !xrtGetError());
    /* FailAfter fails this and every later allocation, including diagnostics.
     * Sweep through the first non-triggered full factory/invocation result. */
    size_t factoryAttempts = 0, invocationAttempts;
    for (; checkRead && factoryAttempts < 64; ++factoryAttempts) {
        xl6ProductMemoryBegin(factoryAttempts); xvalue* made = createReader();
        bool failed = xl6ProductMemoryEnd();
        assert((made != NULL) == !failed); xrtValueRelease(made); xrtClearError(); xl6ProductMemoryVerify();
        if (!failed) break;
    }
    assert(!checkRead || (factoryAttempts > 4 && factoryAttempts < 64));
    callable = createReader(); assert(callable && !xrtGetError());
    xrtValueRelease(receiver); receiver = NULL; assert(!drops);
    inspect(false, false); assert(invoke() && !xrtGetError()); inspect(false, false);
    for (invocationAttempts = 0; invocationAttempts < 64; ++invocationAttempts) {
        xl6ProductMemoryBegin(invocationAttempts); bool ok = invoke(); bool failed = xl6ProductMemoryEnd();
        assert(ok == !failed); xrtClearError(); xl6ProductMemoryVerify(); if (!failed) break;
    }
    assert(invocationAttempts && invocationAttempts < 64);
    parking = true; xthread* thread = xrtThreadCreate(worker, NULL, 0); assert(thread);
    while (!xrtAtomic64Load(&ready, XMEMORY_ACQUIRE)) xrtThreadYield();
    inspect(true, true); xrtAtomic64Store(&proceed, 1, XMEMORY_RELEASE);
    assert(xrtThreadWait(thread) == XWAIT_OK); xrtThreadDestroy(thread); parking = false; inspect(false, false);
    assert(adapterModule->codeLeases && receiverModule->codeLeases);
    xlDestroyModule(adapterModule); assert(adapterModule->destroyRequested);
    xlDestroyModule(receiverModule); assert(receiverModule->destroyRequested);
    /* No independent native caller lease masks the missing adapter residency.
     * The saved callable alone protects A and its receiver alone protects B. */
    assert(invoke() && !xrtGetError()); inspect(false, false);
    xrtValueRelease(callable); callable = NULL; adapterModule = receiverModule = NULL; assert(drops == 1);
    xlDestroyContext(context); xlRuntimeUnit(); xrtClearError();
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == baseline.LiveCount && after.LiveBytes == baseline.LiveBytes);
    assert(after.AllocCount - baseline.AllocCount == after.FreeCount - baseline.FreeCount);
    assert(!after.DoubleFreeCount && !after.InvalidFreeCount && !after.UseAfterFreeCount);
    assert(xrtMemDebugVisitLive(live, NULL)); assert(!checkRead || readChecks > 5);
    printf("actual TCC bound activity: two distinct module owners, %zu continuous factory OOM prefixes, %zu invocation OOM prefixes, %u probes, checked foreign read refusal, closed call and resident last-drop, exact tool-only heap baseline\n",
        factoryAttempts, invocationAttempts, probes);
    return 0;
}
