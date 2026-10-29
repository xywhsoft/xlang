/* Generated module preincluded, or XLIR_NATIVE_PROBE_ONLY for native checks. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/runtime/xl6_text_value.h"
int main(void)
{
    const char input[] = "\xe4\xbd\xa0\xe5\xa5\xbd";
    const char expected[] = "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd";
    int round, count, failure;
    xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true));
#ifndef XLIR_NATIVE_PROBE_ONLY
    assert(xlir_module_init()); xlir_module_fini();
#endif
    for (round = 0; round < 100; ++round)
    for (count = 0; count <= 2; ++count)
    for (failure = 0; failure < 2; ++failure) {
        xstrview text;
        xrtMemDebugSnapshot(&before);
        if (failure) assert(xrtMemDebugFailAfter(0));
        text = xl6TextRepeatV1((xstrview){input, sizeof(input) - 1}, count, -1);
        if (failure) {
            assert(text.Data == NULL && text.Size == 0 && xrtMemDebugFailTriggered());
            assert(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            xrtMemDebugFailClear();
        } else {
            assert(text.Data != NULL && text.Size == (sizeof(input) - 1) * (size_t)count);
            assert(memcmp(text.Data, expected, text.Size) == 0 && text.Data[text.Size] == 0);
        }
        xrtFree((void*)text.Data); xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("string repeat: language values, 600 cases, 300 real allocation failures, balanced memory");
    return 0;
}
