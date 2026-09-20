#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
int expression_case(void);
void expression_finish(void);
void expression_failure_case(void);
int main(void)
{
    unsigned round; xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true));
    /* Warm process error infrastructure explicitly; each subsequent complete
     * module activation/cleanup and each expression scope must be balanced. */
    assert(expression_case()); expression_failure_case(); expression_finish();
    for (round = 0; round < 100; ++round) {
        xrtMemDebugSnapshot(&before);
        assert(expression_case()); expression_failure_case(); expression_finish(); assert(!xrtGetError());
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("xcall expression: 25 forms x 100 rounds + 100 real callable allocation failures; exact evaluation, borrowed source survival, temporary/error cleanup, balanced memory");
    return 0;
}
