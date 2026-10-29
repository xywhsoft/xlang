/* Exercise the real source API transaction with only TCC symbol lookup
 * replaced. No generated module or published compiler DLL is substituted. */
#define XL_STATIC_EMBED
#define XL_ENABLE_EXE_COMPILER 0
#include "src/api/xl_api.c"
#include "src/api/xl_api_path.c"
#include "src/common/xl6_console.c"
#include "src/runtime/xl6_continuation_queue.h"
#include <assert.h>

static unsigned attempts, failures_left, fail_mode;
static int artifact_live;
static unsigned retired, probes;
static int legacy, no_finalizer;
static int retirement_mode;
static XL_Context* current_context;
static XL_Module* current_module;
static unsigned deleted, unsafe_deleted;
static int binding_enabled, binding_fail, probe_worker;
static unsigned bindings;
static XL6_CodeOwner bound_owner;
static unsigned pinned_frames_retired, required_frame_retirements;
static int verify_frames;
static int32_t lock_probe(void* data)
{ (void)data; xlApiLock(); xlApiUnlock(); return 0; }
static int bind_owner(const XL6_CodeOwner* owner)
{
    assert(owner && owner->size == sizeof(*owner) && owner->context && owner->acquire && owner->release);
    assert(!owner->acquire(owner->context)); /* Binding has not committed. */
    bound_owner = *owner; ++bindings; return !binding_fail;
}
static int checked_unit(void)
{
    ++attempts;
    assert(current_module->activation == XL_MODULE_DEACTIVATING ||
        (retirement_mode && current_module->activation == XL_MODULE_INACTIVE));
    /* Every public call reenters the actual API lock/lease path. */
    assert(!xlDeactivateModule(current_context, current_module));
    assert(!xlActivateModule(current_context, current_module, NULL));
    assert(!xlRunModuleMain(current_context, current_module, NULL)); probes += 3;
    if (retirement_mode) {
        xlDestroyModule(current_module); /* No recursive unit/delete. */
        assert(xlRuntimeInit()); xlRuntimeUnit(); /* Collector reentry is bounded. */
    }
    if (probe_worker) {
        xthread* thread; probe_worker = 0;
        thread = xrtThreadCreate(lock_probe, NULL, 0); assert(thread);
        assert(xrtThreadWaitFor(thread, 5000000) == XWAIT_OK); xrtThreadDestroy(thread);
    }
    if (failures_left) {
        --failures_left;
        if (fail_mode == 1) xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "module", 57, "module is busy"));
        if (fail_mode == 2 && artifact_live) { artifact_live = 0; ++retired; }
        return 0;
    }
    if (artifact_live) { artifact_live = 0; ++retired; }
    return 1;
}
static void legacy_unit(void)
{
    if (!checked_unit()) xl6ErrorScopePropagateFailure((xl6_error_result){false, xrtTakeError()});
}
static int has_error(void) { return xrtGetError() != NULL; }
static void clear_error(void) { xrtClearError(); }
static void* lookup(TCCState* state, const char* name)
{
    assert(state == (TCCState*)&artifact_live);
    if (binding_enabled && !strcmp(name, "xl_script_bind_code_owner")) return (void*)bind_owner;
    if (!no_finalizer && !legacy && !strcmp(name, "xl_script_module_unit_checked")) return (void*)checked_unit;
    if (!no_finalizer && !strcmp(name, "xl_script_module_unit")) return (void*)legacy_unit;
    if (!strcmp(name, "xl_script_has_error")) return (void*)has_error;
    if (!strcmp(name, "xl_script_clear_error")) return (void*)clear_error;
    return NULL;
}
static void delete_state(TCCState* state)
{
    assert(state == (TCCState*)&artifact_live);
    if (verify_frames) assert(pinned_frames_retired == required_frame_retirements);
    ++deleted; if (artifact_live) ++unsafe_deleted;
}
typedef struct pinned_frame {
    XL6_ContinuationTicket ticket;
    unsigned index;
} pinned_frame;
static void pinned_run(void*);
static void pinned_release(void*);
static void pinned_run(void* data)
{
    pinned_frame* frame = (pinned_frame*)data;
    assert(current_module->codeLeases == 1 && deleted == 0);
    if (frame->index == 0) {
        pinned_frame* next = (pinned_frame*)xrtCalloc(1, sizeof(*next)); assert(next); next->index = 1;
        assert(xl6ContinuationReserveSuccessor(&frame->ticket, &next->ticket, pinned_run, pinned_release, next) == XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationAbort(&next->ticket) && current_module->codeLeases == 1);
        assert(xl6ContinuationReserveSuccessor(&frame->ticket, &next->ticket, pinned_run, pinned_release, next) == XL6_CONTINUATION_ADMITTED);
        xl6ContinuationReady(&next->ticket); xl6ContinuationTransportReleased(&next->ticket); xl6ContinuationCommit(&next->ticket);
    }
}
static void pinned_release(void* data)
{
    pinned_frame* frame = (pinned_frame*)data;
    assert(pinned_frames_retired == frame->index && current_module->codeLeases == 1 && deleted == 0);
    ++pinned_frames_retired; xrtFree(frame);
}
static int retirement_boundary(void)
{
    unsigned round, mode;
    xmemdebugsnapshot total_before, total_after;
    xrtMemDebugSnapshot(&total_before);
    retirement_mode = 1;
    xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
    for (round = 0; round < 100; ++round)
    for (mode = 0; mode < 4; ++mode) {
        XL_Context context = {0}; XL_Module* module; xerror* ambient;
        xmemdebugsnapshot before, after;
        xrtMemDebugSnapshot(&before);
        module = (XL_Module*)xrtCalloc(1, sizeof(*module)); assert(module);
        current_context = &context; current_module = module;
        module->state = (TCCState*)&artifact_live;
        module->activation = ((round & 2) && mode != 3) ? XL_MODULE_INACTIVE : XL_MODULE_ACTIVE;
        artifact_live = 1; attempts = retired = deleted = unsafe_deleted = 0;
        fail_mode = mode == 3 ? 1 : mode; failures_left = 2; legacy = mode == 3;
        ambient = xrtErrorCreate(XERR_STATE, "caller", 59, "earlier failure"); assert(ambient);
        xrtSetErrorTake(ambient);
        module->activeCalls = 1; xlDestroyModule(module);
        assert(attempts == 0 && deleted == 0 && module->retirementQueued);
        xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        assert(attempts == 1 && deleted == 0 && module->retirementQueued);
        assert(module->activation == XL_MODULE_DEACTIVATION_PENDING && xrtGetError() == ambient);
        assert((module->retirementError != NULL) == (fail_mode == 1));
        xlDestroyModule(module);
        assert(attempts == 2 && deleted == 0 && xrtGetError() == ambient);
        if (round & 1) xlDestroyModule(module);
        else { assert(xlRuntimeInit()); xlRuntimeUnit(); } /* Owner queue retains one-shot failures. */
        assert(deleted == 1 && unsafe_deleted == 0 && artifact_live == 0 && retired == 1);
        assert(xl__retiringModules == NULL && xrtGetError() == ambient);
        xrtFree(context.lastError); xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    }
    for (round = 0; round < 100; ++round) {
        XL_Context context = {0}; XL_Module* module; XL_LoadedLibrary dependency = {0};
        unsigned unloaded = 0, deletes_at_unload = 0; int runtime_before = xl__runtimeManualCount;
        void (*bind_dependency)(unsigned*, unsigned*, unsigned*);
        artifact_live = 1; attempts = retired = deleted = unsafe_deleted = 0;
        fail_mode = 0; failures_left = 0; legacy = 0; binding_enabled = 1; bindings = 0;
        current_context = &context;
        dependency.path = "build/test/xlir_module_retirement_dependency.dll";
        dependency.module = LoadLibraryA(dependency.path); assert(dependency.module);
        {
            FARPROC address = GetProcAddress(dependency.module, "retirement_dependency_bind");
            _Static_assert(sizeof(address) == sizeof(bind_dependency), "Windows procedure pointer ABI");
            memcpy(&bind_dependency, &address, sizeof(bind_dependency));
        }
        assert(bind_dependency); bind_dependency(&unloaded, &deleted, &deletes_at_unload);
        context.loadedLibraries = &dependency; context.loadedLibraryCount = 1;
        xlApiLock(); module = xlCreateModuleFromState(&context, (TCCState*)&artifact_live); xlApiUnlock();
        assert(module && bindings == 1); current_module = module; module->activation = XL_MODULE_ACTIVE;
        assert(module->loadedLibraryCount == 1 && xl__runtimeManualCount == runtime_before + 1);
        FreeLibrary(dependency.module); dependency.module = NULL;
        context.loadedLibraries = NULL; context.loadedLibraryCount = 0;
        assert(unloaded == 0);
        assert(bound_owner.acquire(bound_owner.context) && bound_owner.acquire(bound_owner.context));
        assert(!xlDeactivateModule(&context, module) && attempts == 0);
        xlDestroyModule(module); assert(attempts == 0 && deleted == 0 && unloaded == 0);
        assert(!bound_owner.acquire(bound_owner.context));
        bound_owner.release(bound_owner.context); assert(attempts == 0 && unloaded == 0);
        probe_worker = round == 0;
        bound_owner.release(bound_owner.context); /* Last resident-runtime return boundary. */
        assert(deleted == 1 && unsafe_deleted == 0 && retired == 1 && unloaded == 1 && deletes_at_unload == 1);
        assert(xl__retiringModules == NULL && xl__runtimeManualCount == runtime_before);
        xrtFree(context.lastError); xrtClearError();
    }
    for (round = 0; round < 100; ++round)
    for (mode = 0; mode < 2; ++mode) {
        XL_Context context = {0}; XL_Module* module; XL6_ContinuationQueue* queue;
        pinned_frame* frame; XL6_ContinuationTicket extra = {0};
        artifact_live = 1; attempts = retired = deleted = unsafe_deleted = 0;
        failures_left = 0; legacy = 0; current_context = &context;
        verify_frames = 1; pinned_frames_retired = 0; required_frame_retirements = mode == 0 ? 0 : 2;
        xlApiLock(); module = xlCreateModuleFromState(&context, (TCCState*)&artifact_live); xlApiUnlock();
        assert(module); current_module = module; module->activation = XL_MODULE_ACTIVE;
        queue = xl6ContinuationQueueCreate(1); frame = (pinned_frame*)xrtCalloc(1, sizeof(*frame)); assert(queue && frame);
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
        assert(xl6ContinuationReserveOwned(queue, &frame->ticket, pinned_run, pinned_release, frame, &bound_owner) == XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationReserveOwned(queue, &extra, pinned_run, pinned_release, NULL, &bound_owner) == XL6_CONTINUATION_FULL);
        assert(module->codeLeases == 1);
        if (mode == 0) {
            xlDestroyModule(module); /* Failed launch still has its entering code lease. */
            assert(xl6ContinuationAbort(&frame->ticket) && module->codeLeases == 0 && deleted == 0);
            xrtFree(frame);
            xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        } else {
            xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
            xlDestroyModule(module);
            assert(!bound_owner.acquire(bound_owner.context) && deleted == 0);
            xl6ContinuationQueueClose(queue);
            xl6ContinuationReady(&frame->ticket); xl6ContinuationTransportReleased(&frame->ticket); xl6ContinuationCommit(&frame->ticket);
            assert(xl6ContinuationRunOne(queue) && deleted == 0 && current_module->codeLeases == 1);
            assert(xl6ContinuationRunOne(queue));
        }
        assert(deleted == 1 && unsafe_deleted == 0 && xl__retiringModules == NULL);
        xl6ContinuationQueueClose(queue); assert(xl6ContinuationQueueDestroy(queue));
        verify_frames = 0; xrtFree(context.lastError); xrtClearError();
    }
    {
        XL_Context context = {0}; int runtime_before = xl__runtimeManualCount;
        binding_fail = 1;
        xlApiLock(); assert(!xlCreateModuleFromState(&context, (TCCState*)&artifact_live)); xlApiUnlock();
        assert(xl__runtimeManualCount == runtime_before && xl__retiringModules == NULL);
        xrtFree(context.lastError); binding_fail = 0;
    }
    xrtClearError(); xrtMemDebugSnapshot(&total_after);
    assert(total_before.LiveCount == total_after.LiveCount && total_before.LiveBytes == total_after.LiveBytes);
    assert(total_after.AllocCount - total_before.AllocCount == total_after.FreeCount - total_before.FreeCount);
    assert(total_before.InvalidFreeCount == total_after.InvalidFreeCount && total_before.DoubleFreeCount == total_after.DoubleFreeCount);
    puts("module retirement: 400 checked/legacy active/inactive retry transactions; 100 bound code-owner lifetimes with real dependency unload; 200 owned-ticket abort/successor chains, final frame-return before code deletion; binding rollback, first-error preservation, unlocked cleanup and balanced memory");
    return 0;
}
int main(int argc, char** argv)
{
    unsigned mode, round; xmemdebugsnapshot before, after;
    xl6_error_scope warm = {0};
    assert(xrtMemDebugEnable(true));
    assert(xl6ErrorScopeBegin(&warm)); assert(xl6ErrorScopeEnd(&warm).ok);
    if (argc == 2 && !strcmp(argv[1], "--retirement")) return retirement_boundary();
    assert(argc == 1);
    xl__tcc.tcc_get_symbol = lookup;
    for (round = 0; round < 100; ++round)
    for (mode = 0; mode < 4; ++mode) {
        XL_Context context = {0}; XL_Module module = {0};
        xrtMemDebugSnapshot(&before);
        module.state = (TCCState*)&artifact_live; module.activation = XL_MODULE_ACTIVE;
        current_context = &context; current_module = &module;
        artifact_live = 1; attempts = retired = 0; failures_left = 2; fail_mode = mode == 3 ? 1 : mode;
        legacy = mode == 3;
        /* Another admitted external call still owns a code-entry lease. */
        module.activeCalls = 1;
        assert(!xlDeactivateModule(&context, &module) && attempts == 0);
        assert(module.activeCalls == 1 && module.activation == XL_MODULE_ACTIVE);
        module.activeCalls = 0;
        assert(!xlDeactivateModule(&context, &module));
        assert(attempts == 1 && artifact_live == (mode != 2));
        assert(module.activation == XL_MODULE_DEACTIVATION_PENDING);
        assert(!xlActivateModule(&context, &module, NULL));
        assert(!xlDeactivateModule(&context, &module));
        assert(attempts == 2 && module.activation == XL_MODULE_DEACTIVATION_PENDING);
        assert(xlDeactivateModule(&context, &module));
        if (attempts != 3 || artifact_live) {
            fprintf(stderr, "deactivation retry skipped: mode=%u attempts=%u artifact_live=%d\n", mode, attempts, artifact_live);
            return 1;
        }
        assert(retired == 1 && module.activation == XL_MODULE_INACTIVE);
        assert(xlDeactivateModule(&context, &module) && attempts == 3);
        assert(context.activeCalls == 0 && module.activeCalls == 0);
        xrtFree(context.lastError); xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    }
    {
        XL_Context context = {0}; XL_Module module = {0}; no_finalizer = 1;
        module.state = (TCCState*)&artifact_live; module.activation = XL_MODULE_ACTIVE;
        assert(xlDeactivateModule(&context, &module) && module.activation == XL_MODULE_INACTIVE);
        xrtFree(context.lastError); xrtClearError();
    }
    printf("source API deactivation: 400 retry transactions, busy/false-NULL/retired-but-failed/legacy outcomes, %u reentry rejections, outstanding-entry refusal and balanced allocations\n", probes);
    return 0;
}
