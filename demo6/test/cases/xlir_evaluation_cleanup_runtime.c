#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int evaluation_host_init(void);
extern void evaluation_host_fini(void);
extern int64_t evaluation_host_case(int kind);
extern int64_t evaluation_host_operand_case(int kind);
static xerror *secondary, *primary;
static unsigned dropped, boxes, after, caught;
static int raising;
static int operandMode;
static unsigned operandCalls;
void evaluation_operand_arm(void)
{
    ++operandCalls;
    if (operandMode == 1) xrtSetError(primary);
    else if (operandMode == 2) assert(xrtMemDebugFailAfter(0));
}
void evaluation_drop(void) { ++dropped; if (raising) xrtSetError(secondary); }
void evaluation_box_drop(void) { ++boxes; }
void evaluation_after(int64_t count) { assert(count == 5); ++after; }
void evaluation_caught(void) { ++caught; }
void evaluation_primary(void) { xrtSetError(primary); }
int main(int argc, char** argv)
{
    unsigned round, kind, mode;
    xmemdebugsnapshot initial, before, final;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    primary = xrtErrorCreate(XERR_STATE, "evaluation", 88, "original failure");
    secondary = xrtErrorCreate(XERR_STATE, "evaluation", 77, "cleanup failure");
    assert(primary && secondary && evaluation_host_init());
    for (round = 0; round < (smoke ? 1u : 50u); ++round)
    for (kind = 0; kind < 10; ++kind)
    for (mode = 0; mode < 2; ++mode) {
        int64_t result;
        int expectedError = kind == 5 ? 88 : mode && kind != 2 ? 77 : 0;
        int64_t expectedResult = expectedError ? 0 : mode && kind == 2 ? 17 : kind == 7 ? 29 : 5;
        if (!smoke) xrtMemDebugSnapshot(&before);
        dropped = boxes = after = caught = 0; raising = (int)mode;
        result = evaluation_host_case((int)kind);
        if (round == 0) printf("evaluation %u raising=%u: result=%lld drop=%u boxes=%u after=%u caught=%u error=%d\n",
            kind, mode, (long long)result, dropped, boxes, after, caught,
            xrtGetError() ? xrtErrorCode(xrtGetError()) : 0);
        fflush(stdout);
        assert(result == expectedResult);
        assert(expectedError ? xrtGetError() && xrtErrorCode(xrtGetError()) == expectedError : !xrtGetError());
        assert(dropped == 1 && boxes == (kind == 6 && !mode ? 1u : 0u));
        assert(after == (mode ? 0u : 1u) && caught == (mode && kind == 2 ? 1u : 0u));
        xrtClearError();
        if (!smoke) {
            xrtMemDebugSnapshot(&final);
            assert(before.LiveCount == final.LiveCount && before.LiveBytes == final.LiveBytes);
            assert(final.AllocCount - before.AllocCount == final.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == final.InvalidFreeCount && before.DoubleFreeCount == final.DoubleFreeCount);
            assert(before.UseAfterFreeCount == final.UseAfterFreeCount);
        }
    }
    for (round = 0; round < (smoke ? 1u : 50u); ++round)
    for (kind = 0; kind < 8; ++kind)
    for (mode = 0; mode < (smoke ? 2u : 3u); ++mode) {
        int64_t result;
        if (!smoke) xrtMemDebugSnapshot(&before);
        operandMode = (int)mode; operandCalls = 0; raising = 0;
        result = evaluation_host_operand_case((int)kind);
        if (mode == 2) { assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); }
        assert(operandCalls == 1 && result == (mode ? 0 : 17));
        if (mode == 1) assert(xrtGetError() && xrtErrorCode(xrtGetError()) == 88);
        else if (mode == 2) assert(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        else assert(!xrtGetError());
        xrtClearError();
        if (!smoke) {
            xrtMemDebugSnapshot(&final);
            assert(before.LiveCount == final.LiveCount && before.LiveBytes == final.LiveBytes);
            assert(final.AllocCount - before.AllocCount == final.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == final.InvalidFreeCount && before.DoubleFreeCount == final.DoubleFreeCount);
            assert(before.UseAfterFreeCount == final.UseAfterFreeCount);
        }
    }
    printf("evaluation operands: 8 shapes, %u executions, %u armed string allocation failures; partial enum and receiver rollback\n",
        smoke ? 16u : 1200u, smoke ? 0u : 400u);
    evaluation_host_fini(); xrtErrorFree(primary); xrtErrorFree(secondary);
    assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&final);
        assert(initial.LiveCount == final.LiveCount && initial.LiveBytes == final.LiveBytes);
        assert(initial.InvalidFreeCount == final.InvalidFreeCount && initial.DoubleFreeCount == final.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == final.UseAfterFreeCount);
        printf("evaluation cleanup: 1000 cleanup and 1200 operand executions, %llu balanced allocations\n",
            (unsigned long long)(final.AllocCount - initial.AllocCount));
    }
    return 0;
}
