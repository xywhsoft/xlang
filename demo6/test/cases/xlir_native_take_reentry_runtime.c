#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void xlir_native_take_case(int failed, int replace);
int xlir_native_take_live_handles(void);
void xlir_native_drop_outcome_case(int failure, int primary);

static void drop_outcomes(int smoke)
{
    int iteration, failure, primary;
    xmemdebugsnapshot before, after;
    for (iteration = 0; iteration < 100; ++iteration)
    for (failure = 0; failure < 3; ++failure)
    for (primary = 0; primary < 2; ++primary) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        xlir_native_drop_outcome_case(failure, primary);
        if (!smoke) {
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
            assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
            assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        }
    }
    puts("native destructor: 600 outcomes, diagnostic/false-NULL failure and first-error preservation");
}

int main(int argc, char** argv)
{
    xmemdebugsnapshot before, after;
    int iteration, replace;
    if (argc == 2 && strcmp(argv[1], "--smoke") == 0) {
        for (iteration = 0; iteration < 1000; ++iteration)
            for (replace = 0; replace < 2; ++replace) {
                xlir_native_take_case(1, replace);
                xlir_native_take_case(0, replace);
            }
        assert(xlir_native_take_live_handles() == 0);
        drop_outcomes(1);
        puts("native TAKE smoke: 2000 injected allocation failures, 2000 successes, detached reentry and replacement ownership ok");
        return 0;
    }
    assert(argc == 1 && xrtMemDebugEnable(true));
    xlir_native_take_case(0, 1);
    xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "native-take", 0, "warmup"));
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 1000; ++iteration) {
        for (replace = 0; replace < 2; ++replace) {
            assert(xrtMemDebugFailAfter(0));
            xlir_native_take_case(1, replace);
            assert(xrtMemDebugFailTriggered());
            xrtMemDebugFailClear();
            xrtClearError();
            xlir_native_take_case(0, replace);
        }
    }
    assert(xlir_native_take_live_handles() == 0 && xrtGetError() == NULL);
    xrtMemDebugSnapshot(&after);
    assert(before.Enabled && after.Enabled);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    printf("native TAKE: 2000 real allocation failures, 2000 successes, 6000 native owners closed, %llu balanced XRT allocations\n",
        (unsigned long long)(after.AllocCount - before.AllocCount));
    drop_outcomes(0);
    return 0;
}
