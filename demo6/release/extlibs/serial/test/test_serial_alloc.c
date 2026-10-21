#define XRT_MODULE_MEMORY_DEBUG
#include "../include/serial.h"

/* 故障注入步骤写在断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>



/* 独立进程在首次分配前开启诊断，不修改 xl.dll 宿主的内存策略。 */
int main(void)
{
    unsigned int i;
    unsigned int failures = 0;
    unsigned int successes = 0;
    xmemdebugsnapshot before;
    xmemdebugsnapshot after;
    assert(xrtMemDebugEnable(true));
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (i = 0; i < 32; ++i) {
        void* port;
        xbuffer* result;
        bool failed;
        int64_t written;

        assert(xrtMemDebugFailAfter(i));
        port = xserial_open("LOOPBACK", 9600, 8, 1, 0);
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (port == NULL) { assert(failed); ++failures; }
        else { ++successes; xserial_close(port); }
        xrtClearError();

        /* 分配失败不能消费接收队列。 */
        port = xserial_open("LOOPBACK", 9600, 8, 1, 0);
        assert(port != NULL && xserial_write_text(port, "keep") == 4);
        assert(xrtMemDebugFailAfter(i));
        result = (xbuffer*)xserial_read(port, 4);
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (result == NULL) {
            assert(failed && xserial_available(port) == 4);
            ++failures;
        } else {
            assert(result->Size == 4 && memcmp(result->Data, "keep", 4) == 0);
            assert(xserial_available(port) == 0);
            ++successes;
        }
        xrtBufferDestroy(result);
        xserial_close(port);
        xrtClearError();

        /* 写入失败不能报告成功，也不能留下半条回环数据。 */
        port = xserial_open("LOOPBACK", 9600, 8, 1, 0);
        assert(port != NULL && xrtMemDebugFailAfter(i));
        written = xserial_write_text(port, "keep");
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (written < 0) {
            assert(failed && xserial_available(port) == 0);
            ++failures;
        } else {
            assert(written == 4 && xserial_available(port) == 4);
            ++successes;
        }
        xserial_close(port);
        xrtClearError();
    }
    xrtMemDebugSnapshot(&after);
    assert(failures != 0 && successes != 0);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount);
    assert(after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.OverflowCount == before.OverflowCount);
    assert(after.UnderflowCount == before.UnderflowCount);
    printf("serial allocation checks: rejected=%u completed=%u\n", failures, successes);
    return 0;
}
