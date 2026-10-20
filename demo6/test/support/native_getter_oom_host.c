/* Preinclude unchanged C emitted by the full-source compiler. Implementations
 * are the actual source-build objects, not test replacements. Warm once before
 * injection to exclude process-global lazy setup. One fresh process per logical
 * current-thread allocation prefix; no other-thread or cold-runtime OOM claim. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static bool showLive(const xmemdebugallocation* allocation,void* user)
{
    (void)user;
    fprintf(stderr,"live %p size=%zu at %s:%u\n",allocation->Address,allocation->Size,
        allocation->File?allocation->File:"unknown",allocation->Line);
    return true;
}
int main(int argc,char** argv)
{
    assert(argc==2);char* end=NULL;errno=0;
    unsigned long long point=strtoull(argv[1],&end,10);
    assert(*argv[1]&&*argv[1]!='-'&&end&&!*end&&!errno&&point<=SIZE_MAX);
    assert(xrtMemDebugEnable(true));
    assert(xlir_module_init());xlir_module_fini();assert(!xrtGetError());
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter((size_t)point));
    int initialized=xlir_module_init();bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    xerror* failure=xrtTakeError();
    if(!(hit?!initialized&&xrtErrorKind(failure)==XERR_MEMORY:initialized&&!failure))
        fprintf(stderr,"point=%llu initialized=%d hit=%d error=%d\n",point,initialized,hit,xrtErrorKind(failure));
    assert(hit?!initialized&&xrtErrorKind(failure)==XERR_MEMORY:initialized&&!failure);
    xlir_module_fini();assert(!xrtGetError());xlir_module_fini();assert(!xrtGetError());
    xrtErrorFree(failure);xrtMemDebugSnapshot(&after);
    if(before.LiveCount!=after.LiveCount||before.LiveBytes!=after.LiveBytes)xrtMemDebugVisitLive(showLive,NULL);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount==after.DoubleFreeCount&&before.InvalidFreeCount==after.InvalidFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("native-getter allocation point=%llu hit=%d balanced\n",point,hit);
    return 0;
}
