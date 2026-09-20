#define XRT_MODULE_MEMORY_DEBUG
#define main xlir_network_generated_main
#include XLIR_NETWORK_GENERATED_C
#undef main
#include <assert.h>
#if defined(_WIN32)
#include <windows.h>
#endif

static int network_resources_closed(void)
{
    return xlir_module_initialized == 0 && xlir_continuation_executor == NULL &&
        xl6_net_engine == NULL && xl6_net_resolver == NULL;
}

static int memory_errors_unchanged(const xmemdebugsnapshot* before,
    const xmemdebugsnapshot* after)
{
    return after->InvalidFreeCount == before->InvalidFreeCount &&
        after->DoubleFreeCount == before->DoubleFreeCount &&
        after->UseAfterFreeCount == before->UseAfterFreeCount &&
        after->OverflowCount == before->OverflowCount &&
        after->UnderflowCount == before->UnderflowCount;
}

#if defined(XLIR_NETWORK_BUFFER_COMMAND_PROBE)
/* Keep a real worker alive through its engine, but close its submission gate.
 * A rejected post still owns an initialized embedded event. Neither its stack
 * storage nor a kernel event handle may be abandoned by the shared adapter. */
static int check_rejected_posts(void)
{
    xnetengineconfig config;
    xnetengine* engine;
    xnetworker* worker;
    int rejected = 0, handlesBalanced = 1;
    xrtNetEngineConfigInit(&config);
    config.Workers = 1;
    engine = xrtNetEngineCreate(&config);
    assert(engine != NULL && xrtNetEngineStart(engine));
    worker = xrtNetEngineWorker(engine, 0);
    assert(worker != NULL && xrtNetEngineStop(engine));
    for (int round = 0; round < 16; ++round) {
        xl6_net_buffer_command command = {0};
#if defined(_WIN32)
        DWORD before = 0, after = 0;
        assert(GetProcessHandleCount(GetCurrentProcess(), &before));
#endif
        int accepted = xl6_net_buffer_command_execute(worker, &command);
        if (!accepted && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_CLOSED) ++rejected;
        xrtClearError();
#if defined(_WIN32)
        assert(GetProcessHandleCount(GetCurrentProcess(), &after));
        if (before != after) handlesBalanced = 0;
#endif
    }
    int destroyed = xrtNetEngineDestroy(engine);
    int complete = rejected == 16 && handlesBalanced && destroyed && !xrtGetError();
    printf("{\"RejectedPosts\":%d,\"HandleBalanceChecked\":%s,\"HandlesBalanced\":%s,\"EngineDestroyed\":%s,\"Complete\":%s}\n",
        rejected,
#if defined(_WIN32)
        "true",
#else
        "false",
#endif
        handlesBalanced ? "true" : "false", destroyed ? "true" : "false",
        complete ? "true" : "false");
    return complete;
}
#endif

int main(void)
{
    xmemdebugsnapshot initial, previous, after, final;
    char name[] = "network-lifetime";
    char* arguments[] = {name, NULL};
    int ok = 1, cycles = 0, rejectedPostsComplete = 1;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&initial);
    previous = initial;
    for (int round = 0; round < 5; ++round) {
        /* Execute the saved AOT public main, including checked init/unit and
         * network service teardown, not just the fragment's private fini. */
        int result = xlir_network_generated_main(1, arguments);
        int resources = network_resources_closed();
        xrtMemDebugSnapshot(&after);
        int clean = memory_errors_unchanged(&previous, &after);
        printf("{\"Cycle\":%d,\"Exit\":%d,\"ResourcesClosed\":%s,\"LiveDelta\":%lld,\"ByteDelta\":%lld,\"InvalidFreeDelta\":%llu,\"DoubleFreeDelta\":%llu,\"UseAfterFreeDelta\":%llu,\"OverflowDelta\":%llu,\"UnderflowDelta\":%llu,\"MemoryErrorsUnchanged\":%s}\n",
            round, result, resources ? "true" : "false",
            (long long)after.LiveCount - (long long)previous.LiveCount,
            (long long)after.LiveBytes - (long long)previous.LiveBytes,
            (unsigned long long)(after.InvalidFreeCount - previous.InvalidFreeCount),
            (unsigned long long)(after.DoubleFreeCount - previous.DoubleFreeCount),
            (unsigned long long)(after.UseAfterFreeCount - previous.UseAfterFreeCount),
            (unsigned long long)(after.OverflowCount - previous.OverflowCount),
            (unsigned long long)(after.UnderflowCount - previous.UnderflowCount),
            clean ? "true" : "false");
        fflush(stdout);
        ++cycles;
        if (!clean || (round && (after.LiveCount != previous.LiveCount ||
            after.LiveBytes != previous.LiveBytes))) ok = 0;
        previous = after;
        if (result || !resources || xrtGetError()) { ok = 0; break; }
    }
#if defined(XLIR_NETWORK_BUFFER_COMMAND_PROBE)
    if (network_resources_closed() && !xrtGetError())
        rejectedPostsComplete = check_rejected_posts();
    else rejectedPostsComplete = 0;
#endif
    /* All public entries and callbacks have finished. Only now retire the
     * host thread's runtime storage; the first cycle's allocation is visible
     * above and must also be reclaimed in this final exact accounting. */
    int retired = 0;
    if (network_resources_closed()) retired = xrtRuntimeRetireThreadStorage();
    xrtMemDebugSnapshot(&final);
    int balanced = final.LiveCount == initial.LiveCount && final.LiveBytes == initial.LiveBytes &&
        final.AllocCount - initial.AllocCount == final.FreeCount - initial.FreeCount &&
        memory_errors_unchanged(&initial, &final);
    int complete = ok && cycles == 5 && retired && balanced && rejectedPostsComplete;
    printf("{\"Cycles\":%d,\"ThreadRetired\":%s,\"FinalLiveDelta\":%lld,\"FinalByteDelta\":%lld,\"Balanced\":%s,\"RejectedPostsComplete\":%s,\"Complete\":%s}\n",
        cycles, retired ? "true" : "false",
        (long long)final.LiveCount - (long long)initial.LiveCount,
        (long long)final.LiveBytes - (long long)initial.LiveBytes,
        balanced ? "true" : "false", rejectedPostsComplete ? "true" : "false",
        complete ? "true" : "false");
    return complete ? 0 : 1;
}
