/* Generated binder + actual source API + resident queue. TCC lookups/deletion
 * are recorders: this does not prove actual JIT page unmapping. Each invocation
 * is a fresh artifact lifetime; never reset the generated binding statics. */
#define XL_STATIC_EMBED
#define XL_ENABLE_EXE_COMPILER 0
#include "src/api/xl_api.c"
#include "src/api/xl_api_path.c"
#include "src/common/xl6_console.c"
#include "src/runtime/xl6_continuation_queue.h"
#include "xlir_code_owner_binding.generated.h"
#include <assert.h>
extern const XL6_CodeOwner* test_code_owner(void);
extern int test_code_owner_sealed(void);
extern int xl_script_module_init(xvalue*);
extern int xl_script_module_unit_checked(void);
extern void xl_script_error_unit(void);
static XL_Context context;
static XL_Module* module;
static unsigned probes, runs, releases, deleted;
static int fail_init, rebind_probe, legacy_trace_unavailable;
static XL6_CodeOwner replacement;
__attribute__((externally_visible)) bool code_owner_probe(void)
{
    ++probes;
    assert(test_code_owner_sealed());
    if (rebind_probe) assert(!xl_script_bind_code_owner(&replacement));
    return !fail_init;
}
static void* lookup(TCCState* state, const char* name)
{
    (void)state;
    if (!strcmp(name, "xl_script_bind_code_owner")) return (void*)xl_script_bind_code_owner;
    if (!strcmp(name, "xl_script_module_init")) return (void*)xl_script_module_init;
    if (!strcmp(name, "xl_script_module_unit_checked")) return (void*)xl_script_module_unit_checked;
    if (!strcmp(name, "xl_script_error_unit")) return (void*)xl_script_error_unit;
    if (!strcmp(name, "xl_script_module_ownership_trace"))
        return legacy_trace_unavailable ? NULL : (void*)xl_script_module_ownership_trace;
    return NULL;
}
static void delete_state(TCCState* state)
{
    assert(state == (TCCState*)&context);
    assert(releases == runs && module->codeLeases == 0);
    ++deleted;
}
typedef struct Frame { XL6_ContinuationTicket ticket; unsigned index; } Frame;
static Frame frames[2];
static void run(void* data);
static void release(void* data)
{
    Frame* frame = data;
    assert(!deleted && module->codeLeases == 1 && releases == frame->index);
    ++releases;
}
static void run(void* data)
{
    Frame* frame = data;
    assert(!deleted && module->codeLeases == 1);
    assert(code_owner_step(frame->index) == frame->index + 1); ++runs;
    if (!frame->index) {
        frames[1].index = 1;
        assert(xl6ContinuationReserveSuccessor(&frame->ticket, &frames[1].ticket,
            run, release, &frames[1]) == XL6_CONTINUATION_ADMITTED);
        xl6ContinuationReady(&frames[1].ticket);
        xl6ContinuationTransportReleased(&frames[1].ticket);
        xl6ContinuationCommit(&frames[1].ticket);
    }
}
static int dummy_acquire(void* data) { (void)data; assert(0); return 0; }
static void dummy_release(void* data) { (void)data; assert(0); }
static bool dummy_count(const void* data, size_t* count) { (void)data; (void)count; assert(0); return false; }
static bool dummy_trace(const void* data, xrtownershipvisitor visit, void* context)
{ (void)data; (void)visit; (void)context; assert(0); return false; }
static const xrtownershipops dummy_ops = {dummy_count, dummy_trace};
int main(int argc, char** argv)
{
    XL6_CodeOwner invalid = { sizeof(invalid), NULL, dummy_acquire, dummy_release };
    XL6_ContinuationQueue* queue;
    xmemdebugsnapshot before, after;
    assert(argc == 2); fail_init = !strcmp(argv[1], "fail") || !strcmp(argv[1], "unbound-fail");
    legacy_trace_unavailable = !strcmp(argv[1], "legacy-prefix");
    replacement = invalid; rebind_probe = 1;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(!xl_script_bind_code_owner(NULL));
    invalid.size = sizeof(size_t); assert(!xl_script_bind_code_owner(&invalid));
    invalid = replacement; invalid.acquire = NULL; assert(!xl_script_bind_code_owner(&invalid));
    invalid = replacement; invalid.release = NULL; assert(!xl_script_bind_code_owner(&invalid));
    assert(!test_code_owner()->size && !test_code_owner_sealed());
    if (!strcmp(argv[1], "unbound-fail")) {
        assert(!xl_script_module_init(NULL)); assert(!xl_script_bind_code_owner(&replacement));
        assert(test_code_owner_sealed() && !test_code_owner()->size);
        xrtClearError(); assert(xl_script_module_unit_checked());
    } else if (!strcmp(argv[1], "traced")) {
        XL6_CodeOwnerGraph traced = {replacement, 0x584C36434F574E31ULL, &dummy_ops};
        traced.base.size = sizeof(traced);
        assert(!xl_script_bind_code_owner(&traced.base)); /* nonempty graph identity required */
        assert(!test_code_owner()->size && !test_code_owner_sealed());
        traced.base.context = &context;
        assert(xl_script_bind_code_owner(&traced.base)); memset(&traced, 0, sizeof(traced));
        assert(test_code_owner()->size == sizeof(XL6_CodeOwnerGraph) && test_code_owner()->context == &context);
        assert(xl6CodeOwnerGraphOps(test_code_owner()) == &dummy_ops);
        assert(xl_script_module_init(NULL) && xl_script_module_unit_checked());
    } else if (!strcmp(argv[1], "extended")) {
        struct { XL6_CodeOwner owner; size_t tail[4]; } extended = {0};
        extended.owner = replacement; extended.owner.size = sizeof(extended);
        assert(xl_script_bind_code_owner(&extended.owner));
        memset(&extended, 0, sizeof(extended));
        assert(test_code_owner()->size == sizeof(XL6_CodeOwner) && !test_code_owner()->context);
        assert(test_code_owner()->acquire == dummy_acquire && test_code_owner()->release == dummy_release);
        assert(!xl_script_bind_code_owner(test_code_owner()));
        assert(xl_script_module_init(NULL)); assert(xl_script_module_unit_checked());
    } else if (!strcmp(argv[1], "unbound")) {
        assert(xl_script_module_init(NULL)); assert(!xl_script_bind_code_owner(&replacement));
        assert(xl_script_module_unit_checked()); assert(xl_script_module_init(NULL));
        assert(!xl_script_bind_code_owner(&replacement)); assert(!test_code_owner()->size);
        assert(xl_script_module_unit_checked());
    } else {
        xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context, (TCCState*)&context); xlApiUnlock();
        assert(module && test_code_owner()->context == module && module->codeLeases == 0);
        if (legacy_trace_unavailable) {
            /* An older artifact may copy only the stable four-field prefix.
             * It must never receive an advertised tail it cannot preserve. */
            assert(test_code_owner()->size == sizeof(XL6_CodeOwner) && !xl6CodeOwnerGraphOps(test_code_owner()));
        } else assert(test_code_owner()->size == sizeof(XL6_CodeOwnerGraph) &&
            xl6CodeOwnerGraphOps(test_code_owner()) == &xlModuleOwnershipOps);
        assert(test_code_owner()->acquire == module->codeOwnerGraph.base.acquire &&
            test_code_owner()->release == module->codeOwnerGraph.base.release);
        assert(!xl_script_bind_code_owner(&replacement));
        if (fail_init) {
            assert(!xlActivateModule(&context, module, NULL));
            assert(!xl_script_bind_code_owner(&replacement));
            xrtClearError();
            xlDestroyModule(module); assert(deleted == 1);
        } else {
            assert(xlActivateModule(&context, module, NULL));
            assert(xlDeactivateModule(&context, module));
            assert(!xl_script_bind_code_owner(&replacement));
            assert(xlActivateModule(&context, module, NULL));
            queue = xl6ContinuationQueueCreate(1); assert(queue);
            /* White-box external entry lease surrounds the admission call. */
            xlApiLock(); ++module->activeCalls; xlApiUnlock();
            assert(xl6ContinuationReserveOwned(queue, &frames[0].ticket, run, release,
                &frames[0], test_code_owner()) == XL6_CONTINUATION_ADMITTED);
            xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
            xlDestroyModule(module); assert(!deleted && module->codeLeases == 1);
            assert(!test_code_owner()->acquire(test_code_owner()->context));
            xl6ContinuationQueueClose(queue);
            xl6ContinuationReady(&frames[0].ticket);
            xl6ContinuationTransportReleased(&frames[0].ticket);
            xl6ContinuationCommit(&frames[0].ticket);
            assert(xl6ContinuationRunOne(queue) && !deleted);
            assert(xl6ContinuationRunOne(queue) && deleted == 1);
            assert(runs == 2 && releases == 2 && xl6ContinuationQueueDestroy(queue));
        }
        assert(xl__retiringModules == NULL);
    }
    xrtFree(context.lastError); xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    printf("generated code-owner %s: probes=%u runs=%u releases=%u deleted=%u, balanced memory\n",
        argv[1], probes, runs, releases, deleted);
    return 0;
}
