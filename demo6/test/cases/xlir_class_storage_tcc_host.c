/* Real source-generated classes, including destructor-free and failed early
 * publications. Native retention uses the original Value/Clone/cursor APIs;
 * it never registers roots or supplies an extra code lease. */
#define JOINT_TCC_ASYNC
#define XL_ENABLE_EXE_COMPILER 1
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_class_storage_artifact.c"
#define main unused_async_lifetime_main
#include "xlir_one_shot_tcc_host.c"
#undef main
static xvalue* escaped;
static xvalueiter escaped_cursor;
static unsigned capture_mode, destroyed_count;
static void capture_object(xvalue* value)
{
    assert(value && !escaped && !escaped_cursor.Backing);
    if (capture_mode == 3 || capture_mode == 4) assert(xrtValueIterBegin(value, &escaped_cursor));
    else { escaped = capture_mode >= 5 ? xrtValueClone(value) : xrtValueRetain(value); assert(escaped); }
}
static void destroyed_object(int64_t value)
{ assert(value == 42 && !atomic_load(&deleted)); ++destroyed_count; }
typedef struct GraphWalk { xrtownershipref nodes[32]; size_t count, moduleEdges; } GraphWalk;
static bool visit_owner(xrtownershipref edge, void* data)
{
    GraphWalk* walk = data;
    if (edge.Data == module) {
        assert(edge.Ops == &xlModuleOwnershipOps); ++walk->moduleEdges; return true;
    }
    if (!edge.Data) return true;
    for (size_t i = 0; i < walk->count; ++i)
        if (walk->nodes[i].Data == edge.Data && walk->nodes[i].Ops == edge.Ops) return true;
    assert(walk->count < 32 && edge.Ops && edge.Ops->Trace);
    walk->nodes[walk->count++] = edge;
    return edge.Ops->Trace(edge.Data, visit_owner, walk);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0; provider_init();
    for (unsigned round = 0; round < 25; ++round) for (unsigned mode = 0; mode < 7; ++mode) {
        typedef struct Hooks { void (*capture)(xvalue*); void (*destroyed)(int64_t); } Hooks;
        void (*bind)(const Hooks*); int64_t (*start)(int64_t); void* symbol; int64_t result;
        Hooks hooks = {capture_object, destroyed_object}; GraphWalk graph = {0};
        xmemdebugsnapshot before, after;
        atomic_store(&deleted, 0); atomic_store(&cleanups, 0);
        memset(&context, 0, sizeof(context)); memset(&escaped_cursor, 0, sizeof(escaped_cursor));
        escaped = NULL; capture_mode = mode; destroyed_count = 0;
        xrtClearError(); xrtMemDebugSnapshot(&before); load_artifact();
        symbol = lookup(compiled_state, "class_storage_language_bind"); memcpy(&bind, &symbol, sizeof(bind));
        symbol = lookup(compiled_state, "class_storage_start"); memcpy(&start, &symbol, sizeof(start)); assert(bind && start);
        bind(&hooks); xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock(); assert(module);
        assert(xlActivateModule(&context, module, NULL));
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock(); result = start(mode);
        xlApiLock(); xlModuleEndCallUnlocked(module); assert(module->codeLeases == 1); xlApiUnlock();
        assert(result == (mode && mode % 2 == 0 ? 77 : 42) && !destroyed_count && !xrtGetError());
        assert(escaped || escaped_cursor.Backing);
        xlDestroyModule(module); collect(); assert(!atomic_load(&deleted));
        assert(visit_owner(escaped ? xrtValueOwnership(escaped) : xrtValueIterOwnership(&escaped_cursor), &graph));
        assert(graph.moduleEdges == 1); /* Actual object/backing/lease edge, not aggregate pin inference. */
        xrtValueRelease(escaped); escaped = NULL; xrtValueIterEnd(&escaped_cursor);
        finish(); assert(atomic_load(&deleted) == 1 && destroyed_count == (unsigned)(mode % 2 != 0));
        xrtFree(context.lastError); xrtClearError(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("Generated class storage: 175 real TCC close/finalize/unmap lifetimes, 25 destructor-free owners, 75 committed and 75 failed early publications, native Retain/Clone/cursors, actual module edges, no explicit code guard, exact allocator balance");
    return 0;
}
