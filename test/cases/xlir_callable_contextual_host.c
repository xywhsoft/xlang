/* Execute the actual generated module, including async and exception paths.
 * Code remains resident for this ownership test; this is not an unload gate. */
#if !defined(XLIR_CONTEXTUAL_GENERATED_PREINCLUDED)
#include "xlir_callable_contextual_conversion.c"
#endif
#include <assert.h>
#include <windows.h>

int main(void)
{
    unsigned round;
    xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true));
    for (round = 0; round < 100; ++round) {
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        assert(xlir_module_init());
        assert(xrtGetError() == NULL);
        /* A ready Future may precede the resident worker's final release.
         * Busy destruction is non-destructive: keep code resident and retry. */
#if defined(XLIR_RUNTIME_CONTINUATION)
        {
            DWORD started = GetTickCount();
            while (!xlir_module_fini_body().ok) {
                assert(xrtGetError() == NULL);
                assert(GetTickCount() - started < 15000);
                Sleep(1);
            }
        }
#else
        xlir_module_fini();
#endif
        assert(xrtGetError() == NULL);
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
            fprintf(stderr, "round %u live count %zu -> %zu bytes %zu -> %zu\n",
                round, before.LiveCount, after.LiveCount, before.LiveBytes, after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("contextual callable: 100 module cycles, checked failures and exact memory balance complete");
    return 0;
}
