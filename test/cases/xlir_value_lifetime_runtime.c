#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void lifetime_prepare(void);
void lifetime_run(int mode, int reject);
void lifetime_write_run(int stage, int kind);
void lifetime_finish(void);

int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int iteration, mode, reject, stage, kind, cases = 0, writeCases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    lifetime_prepare();
    for (mode = 1; mode <= 11; ++mode)
        for (reject = 0; reject <= 1; ++reject) lifetime_run(mode, reject);
    for (stage = 0; stage <= 2; ++stage)
        for (kind = 0; kind <= 1; ++kind) lifetime_write_run(stage, kind);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration)
        for (mode = 1; mode <= 11; ++mode)
            for (reject = 0; reject <= 1; ++reject) { lifetime_run(mode, reject); ++cases; }
    for (iteration = 0; iteration < 100; ++iteration)
        for (stage = 0; stage <= 2; ++stage)
            for (kind = 0; kind <= 1; ++kind) { lifetime_write_run(stage, kind); ++writeCases; }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("value lifetime: %d genuinely suspended read/write/safe-index cases, %d RHS/address success/failure cases, %llu balanced allocations\n",
            cases, writeCases, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("value lifetime smoke: %d suspended receiver cases, %d RHS/address cases\n", cases, writeCases);
    lifetime_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.DoubleFreeCount == after.DoubleFreeCount && initial.InvalidFreeCount == after.InvalidFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("value lifetime: module finalization restored initial live allocation baseline\n");
    }
    return 0;
}
