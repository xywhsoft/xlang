/* Preinclude the UNMODIFIED emitted C from the actual compiler product.
 * Runtime implementations come only from one actual DLL/SO. A separately
 * compiled transparent call-site probe retains a private waiter reference
 * before language boxing. This is not unobserved JIT or trimmed AOT execution.
 * One process per current-thread allocation prefix; other threads not injected. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

extern size_t testLanguageProcessRetireObserved(void);
extern unsigned testLanguageCancelledDiagnosticFailures(void);

static bool reportLanguageProcessLive(const xmemdebugallocation* allocation,void* user)
{
    (void)user;
    fprintf(stderr,"live %p size=%zu at %s:%u\n",allocation->Address,allocation->Size,
        allocation->File?allocation->File:"unknown",allocation->Line);
    return true;
}

int main(int argc,char** argv)
{
    assert(argc==1 || argc==2);
    uint64_t point=UINT64_MAX;
    if(argc==2){
        char* end=NULL;errno=0;point=strtoull(argv[1],&end,10);
        assert(*argv[1] && *argv[1]!='-' && end && !*end && !errno);
    }
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    if(point!=UINT64_MAX)assert(xrtMemDebugFailAfter(point));
    int initialized=xlir_module_init();
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    xerror* failure=xrtTakeError();
    unsigned cancelledDiagnostics=testLanguageCancelledDiagnosticFailures();
    bool fallback=false;
#if defined(XL6_PROCESS_LANGUAGE_CANCEL_DIAGNOSTIC)
    fallback=hit && initialized && !failure && cancelledDiagnostics==1;
#endif
    bool valid=hit? (!initialized && xrtErrorKind(failure)==XERR_MEMORY) || fallback : initialized && !failure;
    if(!valid)fprintf(stderr,"language-process point=%llu initialized=%d hit=%d kind=%d message=%s\n",
        (unsigned long long)point,initialized,hit,(int)xrtErrorKind(failure),failure?xrtErrorMessage(failure):"none");
    assert(valid);
    assert(point!=0 || hit);
    size_t children=testLanguageProcessRetireObserved();assert(!xrtGetError());
#if defined(XL6_PROCESS_LANGUAGE_FUTURE)
    xdeadline deadline=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)){
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
#endif
    xlir_module_fini();assert(!xrtGetError() && xlir_module_initialized==0);
#if defined(XL6_PROCESS_LANGUAGE_FUTURE)
    assert(!xlir_continuation_executor);
#endif
    xlir_module_fini();assert(!xrtGetError());
    xrtErrorFree(failure);assert(!xrtGetError());xrtMemDebugSnapshot(&after);
    if(before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes)
        xrtMemDebugVisitLive(reportLanguageProcessLive,NULL);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount && before.InvalidFreeCount==after.InvalidFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("language-process point=%llu hit=%d children=%zu waiter/executor retired balanced=1 "
        "outcome=%s cancelled-diagnostic-failures=%u\n",
        (unsigned long long)point,hit,children,fallback?"cancelled-diagnostic":hit?"memory":"success",cancelledDiagnostics);
    return 0;
}
