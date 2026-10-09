#include "xl.h"
#include "src/api/xl_module_state.h"
#include "xlir_raw_activity_vectors.h"
#ifndef XL6_RAW_ACTIVITY_SOURCE
#define XL6_RAW_ACTIVITY_SOURCE "test/cases/xlir_raw_activity.xl"
#endif

static XL_Module* executing;
static void* toolAddress;
static size_t toolSize;
static bool captureTool;
extern void foreign_raw_int(int32_t (*)(void));
extern void foreign_raw_bool(bool (*)(void));
extern void foreign_raw_double(double (*)(void));
extern void foreign_raw_pointer(void* (*)(void));
extern void foreign_raw_void(void (*)(void));
extern void foreign_raw_save(int32_t (*)(void));
extern void foreign_raw_saved(void);
extern void foreign_raw_success(int32_t (*)(void));
extern void foreign_raw_pair(int32_t (*)(void), int32_t (*)(void));
extern int64_t foreign_raw_count(void);
static bool live(const xmemdebugallocation* allocation, void* unused)
{
    (void)unused;
    if (captureTool) { assert(!toolAddress); toolAddress = allocation->Address; toolSize = allocation->Size; }
    else assert(allocation->Address == toolAddress && allocation->Size == toolSize);
    return true;
}
static void inspectActivity(bool active, bool foreign)
{
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(trackedOwner) == active && xl6CodeOwnerScopeForeign(trackedOwner) == foreign);
    assert(executing->activeCalls == 0);
    if (active) { size_t count; assert(!executing->codeOwnerGraph.ops->Count(executing, &count)); }
    assert(xrtOwnershipScopeEnd(&freeze));
}
int main(int argc, char** argv)
{
    assert(argc == 5); XL_ContextOptions options = xlDefaultContextOptions();
    options.workDir = "."; options.tccDllPath = argv[1]; options.tccLibPath = argv[2]; options.tempDir = argv[3];
    xl6ProductMemoryBegin(SIZE_MAX);
    XL_Context* warm = xlCreateContext(&options); assert(warm && xlAddIncludePath(warm, argv[4]));
    XL_Module* tool = xlCompileCSourceStringToModule(warm, "int b163_tool_probe(void) { return 42; }\n"); assert(tool);
    int (*warmProbe)(void) = (int (*)(void))xlModuleGetSymbol(tool, "b163_tool_probe"); assert(warmProbe() == 42);
    xlDestroyModule(tool); xlDestroyContext(warm); xlRuntimeUnit(); xrtClearError();
    xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline); assert(baseline.LiveCount == 1);
    captureTool = true; assert(xrtMemDebugVisitLive(live, NULL)); captureTool = false;
    XL_Context* context = xlCreateContext(&options); assert(context && xlAddIncludePath(context, argv[4]));
    assert(xlRegisterCFunction(context, "raw_activity_probe", (void*)raw_activity_probe));
    assert(xlRegisterCFunction(context, "raw_activity_save", (void*)raw_activity_save));
    executing = xlCompileScriptFileToModule(context, XL6_RAW_ACTIVITY_SOURCE);
    if (!executing) fprintf(stderr, "%s\n", xlGetLastError(context) ? xlGetLastError(context) : "no diagnostic");
    assert(executing && !executing->ownershipReadContract);
    trackedOwner = &executing->codeOwnerGraph.base;
    void (*save)(void) = (void (*)(void))xlModuleGetSymbol(executing, "capture_raw_activity"); assert(save);
    xl6_foreign_error_scope call = {0}; assert(xl6ForeignCallBegin(&call)); save();
    xl6_foreign_error_result result = xl6ForeignCallEnd(&call); assert(result.ok && !result.error && !depth);
    observing = true; inspectActivity(false, false);
    unsigned vectors = runVectors(); size_t attempts = runOom(); missingDiagnosticOom(); directOom(); foreignCallback();
    XL6_CodeLease* caller = NULL;
    assert(xl6CodeLeaseAcquire(trackedOwner, &caller) == XL6_CODE_LEASE_ACQUIRED);
    xlDestroyModule(executing); assert(executing->destroyRequested);
    /* The independent caller lease is NOT entered as a CodeScope: this must
     * prove the raw callback's own registration, not another active frame. */
    directOom(); observing = false; xl6CodeLeaseRelease(caller); executing = NULL; trackedOwner = NULL;
    /* Existing first-error replacement/clearing, saved pointer and mixed raw
     * signatures still execute through the genuine API and TCC, not a mock. */
#define REGISTER(Name) assert(xlRegisterCFunction(context, #Name, (void*)Name))
    REGISTER(foreign_raw_int); REGISTER(foreign_raw_bool); REGISTER(foreign_raw_double);
    REGISTER(foreign_raw_pointer); REGISTER(foreign_raw_void); REGISTER(foreign_raw_save);
    REGISTER(foreign_raw_saved); REGISTER(foreign_raw_success); REGISTER(foreign_raw_pair); REGISTER(foreign_raw_count);
#undef REGISTER
    assert(xlRunScriptFile(context, "test/cases/xlir_foreign_raw_callback_error.xl"));
    assert(foreign_raw_count() == 9);
    xlDestroyContext(context); xlRuntimeUnit(); xrtClearError();
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == baseline.LiveCount && after.LiveBytes == baseline.LiveBytes);
    assert(after.AllocCount - baseline.AllocCount == after.FreeCount - baseline.FreeCount);
    assert(!after.DoubleFreeCount && !after.InvalidFreeCount && !after.UseAfterFreeCount);
    assert(xrtMemDebugVisitLive(live, NULL));
    assert(xrtAtomic64Load(&enters, XMEMORY_RELAXED) == xrtAtomic64Load(&leaves, XMEMORY_RELAXED));
    printf("actual TCC raw activity: %u saved-callback vectors, %zu closed OOM attempts, foreign callback, closed no-raise zero-allocation calls, nine original first-error vectors, exact tool-only heap baseline\n", vectors, attempts);
    return 0;
}
