#include "xrt_decl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern xvalue* xl6_xrt_os_args(void);

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount);
    assert(before->DoubleFreeCount == after.DoubleFreeCount && before->UseAfterFreeCount == after.UseAfterFreeCount);
    assert(!xrtGetError());
}

int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc > 1 && !strcmp(argv[1], "expect-invalid-utf8")) {
        assert(!xl6_xrt_os_args() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError();
        puts("invalid argv UTF-8 checked rejection ok");
        return 0;
    }
    xvalue* values = xl6_xrt_os_args();
    assert(values && xrtValueCount(values) == (size_t)argc);
    if (argc > 1) {
        xstrview text = {0};
        assert(xrtValueGetString(xrtValueArrayGet(values, 1), &text));
        const char* expected = "preflight-\xe4\xbd\xa0";
        assert(text.Size == strlen(expected) && !memcmp(text.Data, expected, text.Size));
        assert(xrtValueGetString(xrtValueArrayGet(values, 2), &text) && text.Size == 0);
    }
    xrtValueRelease(values);
    /* Initialize the allocation-failure diagnostic before measuring a prefix. */
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6_xrt_os_args());
    xrtMemDebugFailClear();
    xrtClearError();
    size_t failed = 0, succeeded = 0;
    for (size_t point = 0; point < 80; ++point) {
        xmemdebugsnapshot before;
        xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));
        values = xl6_xrt_os_args();
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (!values) { assert(triggered); ++failed; }
        else { assert(!triggered && xrtValueCount(values) == (size_t)argc); ++succeeded; }
        xrtValueRelease(values);
        xrtClearError();
        balanced(&before);
    }
    assert(failed && succeeded);
    puts("OS argv allocation prefixes and ownership ok");
    return 0;
}
