#define MODULE_EXECUTOR_MIXED
#include "xlir_module_executor_host.c"
int main(void)
{
    assert(xrtMemDebugEnable(true)); expected_cleanups = 0;
#ifndef MODULE_EXECUTOR_DLL
    provider_init();
#endif
    for (unsigned round = 0; round < 25; ++round) for (unsigned closing = 0; closing < 2; ++closing) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); cycle_width = 1; expected_cleanups = 1;
        create_cycle_module(0);
        xfuture *boot = NULL, *cleanup = NULL;
        xpromise *bootPromise = xrtPromiseCreate(&boot, NULL), *cleanupPromise = xrtPromiseCreate(&cleanup, NULL);
        assert(bootPromise && cleanupPromise && xrtPromiseResolve(cleanupPromise, &number));
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
        xfuture* output = start_cycle(0, boot, cleanup, true); assert(output && !xrtGetError());
        xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        xrtownershipscope freeze = {0}; quiet_executor(executor_view(), &freeze); assert(xrtOwnershipScopeEnd(&freeze));
        int prepared=0; xdeadline preparationDeadline=xrtDeadlineAfter(5000000);
        while(!prepared){
            xlApiLock(); prepared=xlModulePrepareRetirementUnlocked(module); xlApiUnlock();
            assert(!module->retirementFailed&&!atomic_load(&deleted)&&!atomic_load(&global_drops));
            if(!prepared){assert(!xrtDeadlineExpired(preparationDeadline));xrtThreadYield();}
        }
        /* V3 preflight leaves both graph-owned services untouched while
         * accepted await work can still reach them. Destroy is a DIFFERENT boundary:
         * genuinely new task roots then require, and must fail, fresh code
         * admission. Do not silently turn task allocation into scoped reuse. */
        if (closing) xlDestroyModule(module);
        collect(); assert(!atomic_load(&deleted) && !atomic_load(&global_drops));
        assert(xrtPromiseResolve(bootPromise, &number) && xrtFutureWaitFor(output, 5000000) == XWAIT_OK);
        xfutureresult result = {0}; assert(xrtFutureResult(output, &result));
        if (!prepared || result.State != (closing ? XFUTURE_FAILED : XFUTURE_RESOLVED))
            fprintf(stderr, "Mixed service regression: closing=%u preflight=%d result=%d error=%s\n", closing, prepared, (int)result.State,
                result.Error ? xrtErrorMessage(result.Error) : "none");
        assert(prepared);
        if (closing) assert(result.State == XFUTURE_FAILED && result.Error && strstr(xrtErrorMessage(result.Error), "task code admission failed"));
        else assert(result.State == XFUTURE_RESOLVED && result.Value && *(int64_t*)result.Value == 19);
        if (!closing) xlDestroyModule(module);
        finish(); assert(atomic_load(&afters[0]) == (closing ? 0u : 1u) && !atomic_load(&catches[0]) && atomic_load(&finals[0]) == 1);
        if (!closing) assert(*(int64_t*)xrtFutureValue(output) == 19);
        xrtFutureDestroy(output);
        xrtPromiseDestroy(bootPromise); xrtFutureDestroy(boot); xrtPromiseDestroy(cleanupPromise); xrtFutureDestroy(cleanup);
        balance_cycle(&before);
    }
    assert(transactions == 50);
    puts("Mixed module services: 50 real unloads, 25 open-owner await-to-task completions, 25 closed-owner fresh-task refusals, READY preflight without pool mutation, normal finally/globals, MEM_FREE and exact memory balance");
    return 0;
}
