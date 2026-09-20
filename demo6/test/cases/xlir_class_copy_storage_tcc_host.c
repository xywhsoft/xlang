#define JOINT_TCC_ASYNC
#define XL_ENABLE_EXE_COMPILER 1
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_class_copy_storage_artifact.c"
#define main unused_async_lifetime_main
#include "xlir_one_shot_tcc_host.c"
#undef main
static xvalue* escaped;
static xvalueiter escaped_cursor;
static unsigned capture_mode;
static void capture_object(xvalue* value)
{
    assert(value && !escaped && !escaped_cursor.Backing);
    if (capture_mode % 3 == 2) assert(xrtValueIterBegin(value, &escaped_cursor));
    else { escaped = capture_mode % 3 == 1 ? xrtValueClone(value) : xrtValueRetain(value); assert(escaped); }
}
typedef struct CopyGraph { xrtownershipref nodes[32]; size_t count, moduleEdges; } CopyGraph;
static bool visit_owner(xrtownershipref edge, void* data)
{
    CopyGraph* graph = data;
    if (edge.Data == module) { assert(edge.Ops == &xlModuleOwnershipOps); ++graph->moduleEdges; return true; }
    if (!edge.Data) return true;
    for (size_t i = 0; i < graph->count; ++i)
        if (graph->nodes[i].Data == edge.Data && graph->nodes[i].Ops == edge.Ops) return true;
    assert(graph->count < 32 && edge.Ops && edge.Ops->Trace);
    graph->nodes[graph->count++] = edge;
    return edge.Ops->Trace(edge.Data, visit_owner, graph);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0; provider_init();
    for (unsigned round = 0; round < 25; ++round) for (unsigned mode = 0; mode < 6; ++mode) {
        void (*bind)(void (*)(xvalue*)); int64_t (*start)(int64_t); void* symbol; CopyGraph graph = {0};
        xmemdebugsnapshot before, after; uint64 beforeHash = 0, afterHash = 0;
        atomic_store(&deleted, 0); atomic_store(&cleanups, 0);
        memset(&context, 0, sizeof(context)); memset(&escaped_cursor, 0, sizeof(escaped_cursor));
        escaped = NULL; capture_mode = mode; xrtClearError(); xrtMemDebugSnapshot(&before); load_artifact();
        symbol = lookup(compiled_state, "class_copy_storage_bind"); memcpy(&bind, &symbol, sizeof(bind));
        symbol = lookup(compiled_state, "class_copy_storage_start"); memcpy(&start, &symbol, sizeof(start)); assert(bind && start);
        bind(capture_object); xl__tcc.tcc_get_symbol = lookup; xl__tcc.tcc_delete = delete_state;
        xlApiLock(); module = xlCreateModuleFromState(&context, compiled_state); xlApiUnlock(); assert(module);
        assert(xlActivateModule(&context, module, NULL));
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock(); assert(start(mode) == 42 && !xrtGetError());
        xlApiLock(); xlModuleEndCallUnlocked(module); assert(module->codeLeases == 1); xlApiUnlock();
        assert(escaped || escaped_cursor.Backing);
        if (escaped) assert(xrtValueHash(escaped, &beforeHash));
        xlDestroyModule(module); collect(); assert(!atomic_load(&deleted));
        assert(visit_owner(escaped ? xrtValueOwnership(escaped) : xrtValueIterOwnership(&escaped_cursor), &graph));
        assert(graph.moduleEdges == 1); /* Actual copied-backing/lifetime/lease/module chain. */
        /* Default class identity installs a generated hash adapter. Execute
         * its actual TCC code after Close, not only an aggregate lease check. */
        if (escaped) assert(xrtValueHash(escaped, &afterHash) && beforeHash == afterHash && !xrtGetError());
        xrtValueRelease(escaped); escaped = NULL; xrtValueIterEnd(&escaped_cursor); finish(); assert(atomic_load(&deleted) == 1);
        xrtFree(context.lastError); xrtClearError(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("Generated class copies: 150 real TCC lifetimes, 75 shallow/COW and 75 deep copies, native Retain/Clone/cursors after source destruction, 100 generated hash callbacks after Close, actual module edges and unmap, no explicit code guard, exact allocator balance");
    return 0;
}
