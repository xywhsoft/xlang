/* Verified current-source C is preincluded by the gate. */
#include <assert.h>
int main(void)
{
    int round, count, failure;
    xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true));
    assert(xlir_module_init());
    xlir_module_fini();
    for (round = 0; round < 100; ++round)
    for (count = -1; count <= 1; ++count)
    for (failure = 0; failure < 2; ++failure) {
        char* text;
        xrtMemDebugSnapshot(&before);
        if (failure) assert(xrtMemDebugFailAfter(0));
        text = xl6_xrt_string_repeat("你好", (int64_t)count * 2);
        if (failure) {
            assert(!text && xrtMemDebugFailTriggered());
            xrtMemDebugFailClear();
        } else assert(text && !strcmp(text, count > 0 ? "你好你好" : ""));
        xrtFree(text); xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("string repeat: language values, 600 cases, 300 real allocation failures, balanced memory");
    return 0;
}
