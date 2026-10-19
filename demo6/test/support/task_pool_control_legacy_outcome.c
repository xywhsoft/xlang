/* Exercise the existing real task-frame white-box probe after the source split.
 * Its implementation includes task.c once; the pool control provider remains
 * a separately linked translation unit, as it does in actual deployments. */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_TEMP_MEMORY
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
extern unsigned pool_outcome_ready_watch_probe(void);
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    for(unsigned i=0;i<16;++i){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
        assert(pool_outcome_ready_watch_probe()==25);xrtClearError();xrtMemDebugSnapshot(&after);
        assert(after.LiveCount==before.LiveCount&&after.LiveBytes==before.LiveBytes&&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
            after.InvalidFreeCount==before.InvalidFreeCount&&after.DoubleFreeCount==before.DoubleFreeCount&&
            after.UseAfterFreeCount==before.UseAfterFreeCount);
    }
    puts("Legacy task outcome: 400 isolated ready/registration callbacks, exact memory balance");
    return 0;
}
