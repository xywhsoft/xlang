#include "foreign_xge_input_events.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void inputEventHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
}

void inputEventHeapEnd(int mode)
{
    assert(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount);
    assert(after.ReallocCount == before.ReallocCount && before.LiveCount == after.LiveCount);
    printf("XGE_INPUT_EVENT_HEAP mode=%d alloc=0 free=0 realloc=0\n", mode);
}

void inputEventNativeHot(void)
{
    const unsigned char input[] = {'A', 0, 'B'};
    xge_input_event_info_t info = { .iType = XGE_EVENT_IME_UPDATE };
    inputEventHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        xge_input_event_info_t got;
        unsigned char output[sizeof(input)];
        assert(xgeInputEventPostData(&info, input, sizeof(input)) == XGE_OK);
        assert(xgeInputEventRead(&got, output, sizeof(output)) == XGE_INPUT_READ_READY);
        assert(got.iTextSize == sizeof(input) && output[0] == 'A' && output[1] == 0 && output[2] == 'B');
    }
    inputEventHeapEnd(1);

    inputEventHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        xge_ime_event_info_t info;
        assert(xgeImeEventRead(&info, NULL, 0) == XGE_INPUT_READ_EMPTY && info.iTextSize == 0);
    }
    inputEventHeapEnd(3);
}
