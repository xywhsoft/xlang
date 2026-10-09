/* Link the actual public API/compiler/runtime objects and genuine TCC core.
 * The private state header is the actual production layout, not a mirror.
 * A borrowed symbol still requires a live caller handle/independent lease;
 * execution tracking is not automatic ownership of a raw function pointer. */
#include "xl.h"
#include "src/api/xl_module_state.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static XL_Module* executing;
static int64_t (*callProbe)(int64_t);
static int64_t (*callPure)(int64_t);
static int64_t (*callReenter)(int64_t (*)(void));
static xatomic64 ready = {0}, proceed = {0};
static unsigned probes, nested;
static bool closed;
static void* toolAddress;
static size_t toolSize;
static bool captureTool;
static bool live(const xmemdebugallocation* allocation, void* context)
{
    (void)context;
    if (captureTool) {
        assert(!toolAddress); toolAddress = allocation->Address; toolSize = allocation->Size;
        return true;
    }
    assert(allocation->Address == toolAddress && allocation->Size == toolSize);
    return true;
}
static bool diagnose(const xmemdebugallocation* allocation, void* context)
{
    (void)context;
    fprintf(stderr, "live %p size=%zu at %s:%u\n", allocation->Address, allocation->Size,
        allocation->File ? allocation->File : "unknown", allocation->Line);
    if (allocation->Size < 128) fprintf(stderr, "bounded content: %.*s\n",
        (int)allocation->Size, (const char*)allocation->Address);
    return true;
}

static void inspect(bool foreign)
{
    xrtownershipscope freeze = {0};
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(&executing->codeOwnerGraph.base));
    assert(xl6CodeOwnerScopeForeign(&executing->codeOwnerGraph.base) == foreign);
    assert(executing->codeLeases == 1);
    assert(xrtOwnershipScopeEnd(&freeze));
}

static int64_t exportProbe(int64_t mode)
{
    ++probes;
    inspect(false);
    if (mode == 4) {
        xrtAtomic64Store(&ready, 1, XMEMORY_RELEASE);
        while (!xrtAtomic64Load(&proceed, XMEMORY_ACQUIRE)) xrtThreadYield();
    }
    if (mode == 1) xl6ErrorScopePropagateFailure((xl6_error_result){false, NULL});
    if (mode == 2) xl6ErrorScopePropagateFailure((xl6_error_result){false,
        xrtErrorCreate(XERR_STATE, "export-execution", 161, "body failed")});
    return 35;
}

static void empty(void)
{
    xrtownershipscope freeze = {0};
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(!xl6CodeOwnerScopeActive(&executing->codeOwnerGraph.base));
    assert(!xl6CodeOwnerScopeForeign(&executing->codeOwnerGraph.base));
    assert(executing->codeLeases == 0);
    assert(xrtOwnershipScopeEnd(&freeze));
}

static int64_t reenter(void)
{
    ++nested; inspect(false);
    /* An enclosing RESIDENT caller scope owns the actual accepted pin across
     * this close. Nested exports inherit it; they do not reopen the module. */
    if (!closed) { xlDestroyModule(executing); closed = true; }
    assert(executing->destroyRequested && executing->codeLeases == 1);
    assert(callPure(42) == 42);
    inspect(false); return 42;
}

static int32 worker(void* ignored)
{
    xl6_foreign_error_scope call = {0};
    (void)ignored;
    assert(xl6ForeignCallBegin(&call));
    assert(callProbe(4) == 42);
    xl6_foreign_error_result result = xl6ForeignCallEnd(&call);
    assert(result.ok && !result.error); return 0;
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    XL_ContextOptions options = xlDefaultContextOptions();
    options.workDir = "."; options.tccDllPath = argv[1];
    options.tccLibPath = argv[2]; options.tempDir = argv[3];
    xl6ProductMemoryBegin(SIZE_MAX);
    /* TCC is a process-resident tool, not owned by a script module. Warm its
     * actual load/path once, then capture that exact live identity. Do not
     * silently permit "one arbitrary leftover" or suppress a module leak. */
    XL_Context* warm = xlCreateContext(&options); assert(warm);
    assert(xlAddIncludePath(warm, argv[4]));
    XL_Module* tool = xlCompileCSourceStringToModule(warm, "int b161_tool_probe(void) { return 42; }\n");
    assert(tool);
    int (*toolProbe)(void) = (int (*)(void))xlModuleGetSymbol(tool, "b161_tool_probe");
    assert(toolProbe && toolProbe() == 42);
    xlDestroyModule(tool); xlDestroyContext(warm); xlRuntimeUnit(); xrtClearError();
    xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline);
    assert(baseline.Enabled && baseline.LiveCount == 1);
    captureTool = true; assert(xrtMemDebugVisitLive(live, NULL)); captureTool = false;
    XL_Context* context = xlCreateContext(&options); assert(context);
    assert(xlAddIncludePath(context, argv[4]));
    assert(xlRegisterCFunction(context, "export_execution_probe", (void*)exportProbe));
    executing = xlCompileScriptFileToModule(context, "test/cases/xlir_export_execution.xl");
    if (!executing) fprintf(stderr, "%s\n", xlGetLastError(context) ? xlGetLastError(context) : "no diagnostic");
    assert(executing && xlActivateModule(context, executing, NULL));
    assert(!executing->ownershipReadContract); /* No premature certificate. */
    callProbe = (int64_t (*)(int64_t))xlModuleGetSymbol(executing, "scope_probe");
    callPure = (int64_t (*)(int64_t))xlModuleGetSymbol(executing, "scope_pure");
    callReenter = (int64_t (*)(int64_t (*)(void)))xlModuleGetSymbol(executing, "scope_reenter");
    xvalue* (*callValue)(int64_t) = (xvalue* (*)(int64_t))
        xlModuleGetSymbol(executing, "xl6_export_scope_value");
    void (*callVoid)(int64_t) = (void (*)(int64_t))xlModuleGetSymbol(executing, "scope_void");
    assert(callProbe && callPure && callReenter && callValue && callVoid);
    empty();
    for (int mode = 0; mode < 3; ++mode) for (int kind = 0; kind < 3; ++kind) {
        xl6_foreign_error_scope call = {0}; assert(xl6ForeignCallBegin(&call));
        int64_t scalar = 0; xvalue* value = NULL;
        if (!kind) scalar = callProbe(mode);
        else if (kind == 1) value = callValue(mode);
        else callVoid(mode);
        xl6_foreign_error_result result = xl6ForeignCallEnd(&call);
        assert(result.ok == (mode == 0));
        assert((result.error != NULL) == (mode == 2));
        if (!mode) {
            if (!kind) assert(scalar == 42);
            else if (kind == 1) { int64 n = 0; assert(xrtValueGetInt(value, &n) && n == 7); }
        } else assert(!scalar && !value);
        xrtErrorFree(result.error); xrtValueRelease(value); empty();
    }
    /* Complete contiguous operation prefixes, real allocator and rollback,
     * with a fresh baseline around the still-live module for EACH attempt. */
    size_t attempts = 0;
    for (size_t point = 0; point < 64; ++point) {
        unsigned before = probes; bool hit;
        xl6_foreign_error_scope call = {0}; assert(xl6ForeignCallBegin(&call));
        xl6ProductMemoryBegin(point);
        xvalue* value = callValue(0);
        hit = xl6ProductMemoryEnd(); ++attempts;
        xl6_foreign_error_result result = xl6ForeignCallEnd(&call);
        if (hit) { assert(!result.ok && !value); if (!point) assert(probes == before); }
        else { assert(result.ok && value); }
        xrtErrorFree(result.error); xrtValueRelease(value); empty(); xl6ProductMemoryVerify();
        if (!hit) break;
        assert(point + 1 < 64);
    }
    xrtThreadYield(); /* thread implementation initialized before rendezvous */
    xthread* thread = xrtThreadCreate(worker, NULL, 0); assert(thread);
    while (!xrtAtomic64Load(&ready, XMEMORY_ACQUIRE)) xrtThreadYield();
    inspect(true);
    xrtAtomic64Store(&proceed, 1, XMEMORY_RELEASE);
    assert(xrtThreadWait(thread) == XWAIT_OK); xrtThreadDestroy(thread); empty();
    /* The external caller's explicit entry capability outlives every generated
     * frame. Its last release/unmapping happens here, in resident host code. */
    XL6_CodeLease* caller = NULL; XL6_CodeScope entry = {0};
    assert(xl6CodeLeaseAcquire(&executing->codeOwnerGraph.base, &caller) == XL6_CODE_LEASE_ACQUIRED);
    xl6CodeScopeEnter(&entry, caller); xl6CodeLeaseRelease(caller);
    assert(callReenter(reenter) == 42 && closed && nested == 1);
    inspect(false);
    xl6CodeScopeLeave(&entry); executing = NULL;
    xlDestroyContext(context); xlRuntimeUnit(); xrtClearError();
    /* Each per-attempt ledger above is independent. The complete host run
     * returns to the exact tool-only identity captured BEFORE this module. */
    xmemdebugsnapshot final; xrtMemDebugSnapshot(&final);
    if (!final.Enabled || final.LiveCount != baseline.LiveCount || final.LiveBytes != baseline.LiveBytes) {
        fprintf(stderr, "final heap enabled=%d count=%llu bytes=%llu\n", (int)final.Enabled,
            (unsigned long long)final.LiveCount, (unsigned long long)final.LiveBytes);
        xrtMemDebugVisitLive(diagnose, NULL);
    }
    assert(final.Enabled && final.LiveCount == baseline.LiveCount && final.LiveBytes == baseline.LiveBytes);
    assert(final.AllocCount - baseline.AllocCount == final.FreeCount - baseline.FreeCount);
    assert(xrtMemDebugVisitLive(live, NULL));
    assert(!final.DoubleFreeCount && !final.InvalidFreeCount && !final.UseAfterFreeCount);
    printf("actual TCC exports: 9 error/result vectors, %zu closed OOM attempts, foreign thread, close/reentry and exact tool-only heap baseline\n", attempts);
    return 0;
}
