/* The actual generated module is preincluded. One process per prefix point,
 * current-thread logical failpoints; child/waiter allocations on other threads
 * are not described as injected. No public waiter-join guarantee is assumed. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
extern size_t testProcessRetireObserved(void);
static bool reportLiveProcessAllocation(const xmemdebugallocation* allocation,void* user)
{
    (void)user;
    fprintf(stderr,"live %p size=%zu at %s:%u\n",allocation->Address,allocation->Size,
        allocation->File ? allocation->File : "unknown",allocation->Line);
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
    const xerror* error=xrtGetError();
    if(hit ? initialized || xrtErrorKind(error)!=XERR_MEMORY : !initialized || error)
        fprintf(stderr,"point=%zu initialized=%d hit=%d kind=%d message=%s\n",point,
            initialized,hit,(int)xrtErrorKind(error),error ? xrtErrorMessage(error) : "none");
    assert(hit ? !initialized && xrtErrorKind(error)==XERR_MEMORY : initialized && !error);
    xlir_module_fini(); xrtClearError();
    size_t children=testProcessRetireObserved(); xrtClearError();
    xrtMemDebugSnapshot(&after);
    if(before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes)
        xrtMemDebugVisitLive(reportLiveProcessAllocation,NULL);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount && before.InvalidFreeCount==after.InvalidFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("live process language prefix point=%zu hit=%d children=%zu waiter retired balanced\n",point,hit,children);
    return 0;
}
