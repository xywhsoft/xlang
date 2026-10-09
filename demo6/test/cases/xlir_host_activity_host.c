/* Actual full API/compiler/runtime objects and genuine TCC. Wrappers only
 * observe the real resident calls, then always delegate; no registry mirror. */
#include "xl.h"
#include "src/api/xl_module_state.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static _Thread_local unsigned depth;
static xatomic64 enters = {0}, leaves = {0}, ready = {0}, proceed = {0};
static XL_Module* executing;
static int failurePhase;
static bool park;
static unsigned phases[4];
static void* toolAddress;
static size_t toolSize;
static bool capture;
extern void __real_xl6CodeActivityEnterV1(XL6_CodeActivityV1*, const XL6_CodeOwner*);
extern void __real_xl6CodeActivityLeaveV1(XL6_CodeActivityV1*);
static void activityBalance(const xmemdebugsnapshot* before, bool failed)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->AllocCount == after.AllocCount && before->FreeCount == after.FreeCount);
    assert(before->ReallocCount == after.ReallocCount && failed == xrtMemDebugFailTriggered());
}
void __wrap_xl6CodeActivityEnterV1(XL6_CodeActivityV1* scope, const XL6_CodeOwner* owner)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); bool failed = xrtMemDebugFailTriggered();
    __real_xl6CodeActivityEnterV1(scope, owner);
    activityBalance(&before, failed);
    ++depth; xrtAtomic64FetchAdd(&enters, 1, XMEMORY_RELAXED);
}
void __wrap_xl6CodeActivityLeaveV1(XL6_CodeActivityV1* scope)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); bool failed = xrtMemDebugFailTriggered();
    assert(depth); --depth; xrtAtomic64FetchAdd(&leaves, 1, XMEMORY_RELAXED);
    __real_xl6CodeActivityLeaveV1(scope);
    activityBalance(&before, failed);
}
static bool live(const xmemdebugallocation* allocation, void* ignored)
{
    (void)ignored;
    if (capture) { assert(!toolAddress); toolAddress = allocation->Address; toolSize = allocation->Size; }
    else assert(allocation->Address == toolAddress && allocation->Size == toolSize);
    return true;
}
static void inspect(bool active, bool foreign)
{
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(&executing->codeOwnerGraph.base) == active);
    assert(xl6CodeOwnerScopeForeign(&executing->codeOwnerGraph.base) == foreign);
    if (active) { size_t count; assert(!executing->codeOwnerGraph.ops->Count(executing, &count)); }
    assert(xrtOwnershipScopeEnd(&freeze));
}
static int64_t probe(int64_t phase)
{
    assert(phase > 0 && phase < 4 && depth); ++phases[phase]; inspect(true, false);
    if (park && phase == 2) {
        xrtAtomic64Store(&ready, 1, XMEMORY_RELEASE);
        while (!xrtAtomic64Load(&proceed, XMEMORY_ACQUIRE)) xrtThreadYield();
    }
    if (phase == failurePhase) xl6ErrorScopePropagateFailure((xl6_error_result){false, NULL});
    return 42;
}
static int (*callMain)(xvalue*, int*);
static int32 worker(void* ignored)
{
    (void)ignored; int code = -1;
    assert(callMain(NULL, &code) && code == 42 && !depth); return 0;
}
int main(int argc, char** argv)
{
    assert(argc == 5);
    XL_ContextOptions options = xlDefaultContextOptions();
    options.workDir = "."; options.tccDllPath = argv[1]; options.tccLibPath = argv[2]; options.tempDir = argv[3];
    xl6ProductMemoryBegin(SIZE_MAX);
    XL_Context* warm = xlCreateContext(&options); assert(warm && xlAddIncludePath(warm, argv[4]));
    XL_Module* tool = xlCompileCSourceStringToModule(warm, "int b162_tool_probe(void) { return 42; }\n"); assert(tool);
    int (*warmProbe)(void) = (int (*)(void))xlModuleGetSymbol(tool, "b162_tool_probe"); assert(warmProbe() == 42);
    xlDestroyModule(tool); xlDestroyContext(warm); xlRuntimeUnit(); xrtClearError();
    xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline); assert(baseline.LiveCount == 1);
    capture = true; assert(xrtMemDebugVisitLive(live, NULL)); capture = false;
    size_t attempts = 0;
    for (int synthetic = 0; synthetic < 2; ++synthetic) for (int fail = 0; fail < 4; ++fail) {
        failurePhase = fail; memset(phases, 0, sizeof(phases));
        XL_Context* context = xlCreateContext(&options); assert(context && xlAddIncludePath(context, argv[4]));
        assert(xlRegisterCFunction(context, "activity_probe", (void*)probe));
        executing = xlCompileScriptFileToModule(context, synthetic ? "test/cases/xlir_host_activity_synthetic.xl"
            : "test/cases/xlir_host_activity_main.xl");
        if (!executing) fprintf(stderr, "%s\n", xlGetLastError(context) ? xlGetLastError(context) : "no diagnostic");
        assert(executing && executing->ownershipReadContract);
        assert(executing->ownershipReadContract->trace == executing->ownershipTrace);
        int (*init)(xvalue*) = (int (*)(xvalue*))xlModuleGetSymbol(executing, "xl_script_module_init");
        int (*unit)(void) = (int (*)(void))xlModuleGetSymbol(executing, "xl_script_module_unit_checked");
        callMain = (int (*)(xvalue*, int*))xlModuleGetSymbol(executing, "xl_script_main_checked");
        const XL6_ModuleRetirementV2* (*getRetire)(void) = (const XL6_ModuleRetirementV2* (*)(void))
            xlModuleGetSymbol(executing, "xl_script_module_retirement_v2");
        assert(init && unit && callMain && getRetire);
        xl6_foreign_error_scope call = {0}; assert(xl6ForeignCallBegin(&call));
        int initialized = init(NULL);
        xl6_foreign_error_result result = xl6ForeignCallEnd(&call);
        bool initFails = fail == 1 || (synthetic && fail == 2);
        assert(initialized == !initFails && result.ok == !initFails && !result.error && !depth);
        inspect(false, false);
        if (initialized) {
            int code = -1; assert(xl6ForeignCallBegin(&call));
            int ran = callMain(NULL, &code); result = xl6ForeignCallEnd(&call);
            /* These checked host adapters return an explicit bool outcome;
             * ending their private cleanup does not publish it as an outer
             * foreign-call failure event. The public host consumes the bool. */
            assert(ran == (fail != 2) && (!ran || result.ok) && !result.error && !depth);
            assert(code == (fail == 2 ? 1 : synthetic ? 0 : 42)); inspect(false, false);
        }
        if (!fail) {
            uint64_t entered = xrtAtomic64Load(&enters, XMEMORY_RELAXED);
            assert(getRetire()->prepare() == XL6_MODULE_PREPARE_READY && !depth);
            assert(xrtAtomic64Load(&enters, XMEMORY_RELAXED) == entered + 1);
            inspect(false, false);
            if (!synthetic) {
                for (size_t point = 0; point < 128; ++point) {
                    assert(xl6ForeignCallBegin(&call)); xl6ProductMemoryBegin(point);
                    int code = -1; int ran = callMain(NULL, &code); bool hit = xl6ProductMemoryEnd(); ++attempts;
                    result = xl6ForeignCallEnd(&call);
                    /* checked-main's bool is the independent public outcome;
                     * argument acquisition can reject before any language
                     * error scope exists. Foreign-scope OK is not its status. */
                    assert(ran == !hit && (hit || result.ok) && code == (hit ? 1 : 42));
                    if (!hit) assert(!result.error);
                    xrtErrorFree(result.error);
                    inspect(false, false); xl6ProductMemoryVerify();
                    if (!hit) break;
                    assert(point + 1 < 128);
                }
                park = true; xthread* thread = xrtThreadCreate(worker, NULL, 0); assert(thread);
                while (!xrtAtomic64Load(&ready, XMEMORY_ACQUIRE)) xrtThreadYield();
                inspect(true, true); xrtAtomic64Store(&proceed, 1, XMEMORY_RELEASE);
                assert(xrtThreadWait(thread) == XWAIT_OK); xrtThreadDestroy(thread); park = false;
                inspect(false, false);
            }
            /* Close real admission while an independent resident caller pin
             * keeps generated frames alive. Activity is allocation-free even
             * if the existing best-effort host retirement graph preparation
             * attempts allocation when the global object's last lease drops. */
            XL6_CodeLease* caller = NULL; XL6_CodeScope entry = {0};
            assert(xl6CodeLeaseAcquire(&executing->codeOwnerGraph.base, &caller) == XL6_CODE_LEASE_ACQUIRED);
            xl6CodeScopeEnter(&entry, caller); xl6CodeLeaseRelease(caller); xlDestroyModule(executing);
            assert(executing->destroyRequested);
            assert(xl6ForeignCallBegin(&call)); assert(xrtMemDebugFailAfter(0));
            assert(unit()); xrtMemDebugFailClear();
            result = xl6ForeignCallEnd(&call); assert(result.ok && !result.error && !depth && phases[3] == 1);
            xl6CodeScopeLeave(&entry); executing = NULL;
        } else {
            assert(xl6ForeignCallBegin(&call)); int retired = unit(); result = xl6ForeignCallEnd(&call);
            assert(retired == (fail != 3) && (!retired || result.ok) && !result.error && !depth);
            inspect(false, false); xlDestroyModule(executing); executing = NULL;
        }
        xlDestroyContext(context); xlRuntimeUnit(); xrtClearError();
        xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
        assert(after.LiveCount == baseline.LiveCount && after.LiveBytes == baseline.LiveBytes);
        assert(after.AllocCount - baseline.AllocCount == after.FreeCount - baseline.FreeCount);
        assert(!after.DoubleFreeCount && !after.InvalidFreeCount && !after.UseAfterFreeCount);
        assert(xrtMemDebugVisitLive(live, NULL));
    }
    assert(xrtAtomic64Load(&enters, XMEMORY_RELAXED) == xrtAtomic64Load(&leaves, XMEMORY_RELAXED));
    printf("actual TCC host activity: eight init/main/fini vectors, %zu closed OOM attempts, prepare, foreign main, closed OOM cleanup with zero-allocation activity and exact tool-only heap baseline\n", attempts);
    return 0;
}
