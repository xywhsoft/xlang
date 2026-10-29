/* Transparent CALL-SITE observation of emitted C -> genuine resident product.
 * GNU --wrap applies to these consumer references, never to calls inside the
 * DLL/SO. Forward once, return the identical handle/result/error, and retain a
 * real native process before managed boxing can fail. No provider implementation
 * or synthetic process, stream, allocator, Future or completion is installed.
 * The matching full snapshot supplies the PRIVATE waiter layout. Keep loaded. */
#include "release/include/xl_clock.h"
#include "test/support/process_product_observer.h"
#include <assert.h>
#include <stdio.h>

extern void* __real_xl6ProcessSpawnV1(void*);
extern bool __real_xl6PayloadObserveOutcome(int,const xerror*);
static xprocess* observed[8];
static size_t observedCount;
static _Thread_local unsigned cancelledDiagnosticFailures;

void* __wrap_xl6ProcessSpawnV1(void* command)
{
    void* result=__real_xl6ProcessSpawnV1(command);
    if(result){
        XL6_ProcessLive* process=result;
        assert(observedCount<sizeof(observed)/sizeof(*observed));
        observed[observedCount]=xrtProcessRef(process->native);
        assert(observed[observedCount]);++observedCount;
    }
    return result;
}

bool __wrap_xl6PayloadObserveOutcome(int state,const xerror* error)
{
    bool prior=xrtMemDebugFailTriggered();
    bool result=__real_xl6PayloadObserveOutcome(state,error);
    /* In the pinned implementation this exact branch builds an optional
     * cancellation diagnostic and falls back to the CANCELLED singleton.
     * Observe the real result; do not allow arbitrary success after OOM. */
    if(state==XFUTURE_CANCELLED && !result && !prior &&
        xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError())==XERR_CANCELLED)
        ++cancelledDiagnosticFailures;
    return result;
}
unsigned testLanguageCancelledDiagnosticFailures(void)
{return cancelledDiagnosticFailures;}

size_t testLanguageProcessRetireObserved(void)
{
    size_t count=observedCount;
    for(size_t i=0;i<count;++i){
        xprocess* process=observed[i];
        /* Failed language assembly can otherwise leave the real echo child
         * waiting for EOF because the TEST owns an additional user reference. */
        assert(xrtProcessClose(process,XPROCESS_STDIN));
        assert(xrtProcessWaitFor(process,xl6DeadlineRemainingMillisecondsV1(xl6DeadlineAfterMillisecondsV1(5000)))==XWAIT_OK);
        assert(xrtThreadWaitFor(process->Waiter,xl6DeadlineRemainingMillisecondsV1(xl6DeadlineAfterMillisecondsV1(5000)))==XWAIT_OK);
        xrtProcessDestroy(process);observed[i]=NULL;
    }
    observedCount=0;return count;
}
