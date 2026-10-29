#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int match_cleanup_host_init(void);
extern void match_cleanup_host_fini(void);
extern int64_t match_cleanup_host_case(int kind);
static xerror *primary, *secondary;
static unsigned dropped, holders, after, caught, finals;
static int raising, expectedCount;
void match_cleanup_drop(void) { ++dropped; if (raising) xrtSetError(secondary); }
void match_cleanup_holder_drop(void) { ++holders; }
void match_cleanup_after(int64_t count) { assert(count == expectedCount); ++after; }
void match_cleanup_primary(void) { xrtSetError(primary); }
void match_cleanup_caught(void) { ++caught; }
void match_cleanup_finally(void) { ++finals; }
int main(int argc, char** argv)
{
    unsigned round, kind, mode;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot initial, before, final;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    primary = xrtErrorCreate(XERR_STATE, "match-cleanup", 88, "original failure");
    secondary = xrtErrorCreate(XERR_STATE, "match-cleanup", 77, "payload cleanup failure");
    assert(primary && secondary && match_cleanup_host_init());
    for (round = 0; round < (smoke ? 1u : 50u); ++round)
    for (kind = 0; kind < 12; ++kind)
    for (mode = 0; mode < 2; ++mode) {
        int64_t result;
        int expectedError = kind == 4 || kind == 6 ? 88 : mode && kind != 5 ? 77 : 0;
        int64_t expectedResult;
        expectedCount = kind == 0 || kind == 5 || kind == 6 || kind == 10 ? 10 : 5;
        expectedResult = expectedError ? 0 : mode && kind == 5 ? 17 : expectedCount;
        dropped = holders = after = caught = finals = 0; raising = (int)mode;
        if (!smoke) xrtMemDebugSnapshot(&before);
        result = match_cleanup_host_case((int)kind);
        if (round == 0) printf("match cleanup %u raising=%u: result=%lld drops=%u holders=%u after=%u caught=%u finally=%u error=%d\n",
            kind, mode, (long long)result, dropped, holders, after, caught, finals,
            xrtGetError() ? xrtErrorCode(xrtGetError()) : 0);
        fflush(stdout);
        assert(result == expectedResult);
        assert(expectedError ? xrtGetError() && xrtErrorCode(xrtGetError()) == expectedError : !xrtGetError());
        assert(dropped == (kind == 10 ? 4u : kind == 9 && !mode ? 3u : 2u));
        assert(holders == (kind == 11 && !mode ? 1u : 0u));
        assert(after == (!mode && kind != 4 ? 1u : 0u));
        assert(caught == (mode && kind == 5 ? 1u : 0u) && finals == (kind == 7 ? 1u : 0u));
        xrtClearError();
        if (!smoke) {
            xrtMemDebugSnapshot(&final);
            if (before.LiveCount != final.LiveCount || before.LiveBytes != final.LiveBytes)
                fprintf(stderr, "match cleanup %u/%u live delta %lld bytes %lld\n", kind, mode,
                    (long long)(final.LiveCount - before.LiveCount), (long long)(final.LiveBytes - before.LiveBytes));
            assert(before.LiveCount == final.LiveCount && before.LiveBytes == final.LiveBytes);
            assert(final.AllocCount - before.AllocCount == final.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == final.InvalidFreeCount && before.DoubleFreeCount == final.DoubleFreeCount);
            assert(before.UseAfterFreeCount == final.UseAfterFreeCount);
        }
    }
    match_cleanup_host_fini(); xrtErrorFree(primary); xrtErrorFree(secondary);
    assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&final);
        assert(initial.LiveCount == final.LiveCount && initial.LiveBytes == final.LiveBytes);
        printf("match cleanup: 1200 executions, %llu balanced allocations\n",
            (unsigned long long)(final.AllocCount - initial.AllocCount));
    }
    return 0;
}
