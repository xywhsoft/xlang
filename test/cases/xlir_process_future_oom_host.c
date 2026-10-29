/* Preinclude the actual generated program, link one real XRT/resident runtime.
 * One executable per current-thread logical allocation prefix. The observer
 * owns a native reference solely to supply real EOF and join private waiter
 * tails; this is not a public Process.wait/module-fini guarantee. Never clear
 * or free generated globals while a continuation can still access them. */
#include "release/include/xl_clock.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
extern size_t testProcessRetireObserved(void);
extern xerror* __real_xrtErrorBuild(const xerrordesc*);
static _Thread_local size_t cancelledDiagnosticFailures;
xerror* __wrap_xrtErrorBuild(const xerrordesc* description)
{
    /* The cancellation outcome predates its optional diagnostic allocation.
     * The resident observer correctly preserves CANCELLED if that allocation
     * fails. Admit this one explicitly witnessed failure, never arbitrary
     * successful initialization after an injected allocation failure. */
    bool prior=xrtMemDebugFailTriggered();
    xerror* result=__real_xrtErrorBuild(description);
    if(description && description->Kind==XERR_CANCELLED && !result &&
        !prior && xrtMemDebugFailTriggered()) ++cancelledDiagnosticFailures;
    return result;
}
static bool reportFutureLive(const xmemdebugallocation* allocation, void* context)
{
    (void)context;
    fprintf(stderr,"live %p size=%zu at %s:%u\n",allocation->Address,
        allocation->Size,allocation->File ? allocation->File : "unknown",allocation->Line);
    return true;
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==2);
    size_t point=argc==2 ? (size_t)strtoull(argv[1],NULL,10) : SIZE_MAX;
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    if(point!=SIZE_MAX) assert(xrtMemDebugFailAfter(point));
    int initialized=xlir_module_init();
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xerror* failure=xrtTakeError();
    bool cancelledDiagnostic=hit && initialized && !failure && cancelledDiagnosticFailures==1;
    bool valid=hit ? (!initialized && xrtErrorKind(failure)==XERR_MEMORY) || cancelledDiagnostic
                  : initialized && !failure;
    if(!valid)
        fprintf(stderr,"point=%zu initialized=%d hit=%d kind=%d message=%s\n",point,
            initialized,hit,(int)xrtErrorKind(failure),failure ? xrtErrorMessage(failure) : "none");
    assert(valid);
    size_t children=testProcessRetireObserved();
    assert(!xrtGetError());
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)) {
        assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
    }
    xlir_module_fini();
    assert(!xrtGetError() && xlir_module_initialized==0 && !xlir_continuation_executor);
    /* A completed teardown is idempotent; no second finalization is hidden by
     * merely ignoring a BUSY result or clearing its error. */
    xlir_module_fini(); assert(!xrtGetError());
    xrtErrorFree(failure); xrtClearError(); xrtMemDebugSnapshot(&after);
    if(before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes)
        xrtMemDebugVisitLive(reportFutureLive,NULL);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount && before.InvalidFreeCount==after.InvalidFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("process Future language prefix point=%zu hit=%d children=%zu waiter/executor retired balanced; "
        "outcome=%s cancelled-diagnostic-failures=%zu\n",point,hit,children,
        cancelledDiagnostic ? "cancelled-diagnostic" : hit ? "memory" : "success",cancelledDiagnosticFailures);
    return 0;
}
