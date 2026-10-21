#define XRT_MODULE_THREAD
#include "../include/aho_corasick_native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct worker {
    xrtaho* Automaton;
    int Index;
    xatomic32* Ready;
    xatomic32* Start;
} worker;

/* 自动机只读共享；结果和分块扫描器各线程独占。 */
static int run_worker(void* data)
{
    worker* w = data;
    xrtAtomic32FetchAdd(w->Ready, 1u, XMEMORY_RELEASE);
    while (!xrtAtomic32Load(w->Start, XMEMORY_ACQUIRE)) { xrtThreadYield(); }
    for (int i = 0; i < 250; ++i) {
        void* result = xac_search(w->Automaton, "ushers", 6);
        void* stream = xac_stream_create(w->Automaton);
        assert(result && stream && xac_result_count(result) == 3);
        assert(xac_result_start(result, 0) == 1 && xac_result_end(result, 0) == 4);
        xbuffer* pattern = xac_result_pattern(result, 0);
        assert(pattern && pattern->Size == 3 && memcmp(pattern->Data, "she", 3) == 0);
        xrtBufferDestroy(pattern);
        assert(xac_stream_feed(stream, "us", 2) && xac_stream_next(stream) == 0);
        assert(xac_stream_feed(stream, "hers", 4));
        int count = 0, status;
        while ((status = xac_stream_next(stream)) == 1) {
            ++count;
            xrtThreadYield();
        }
        assert(status == 0 && count == 3 && xac_stream_offset(stream) == 6);
        xac_stream_destroy(stream);
        xac_result_destroy(result);

        /* 不同线程的错误槽不能相互覆盖。 */
        xrtSetErrorInfo(XERR_VALUE, "aho.thread.test", w->Index, "thread-local marker");
        xrtThreadYield();
        assert(xrtErrorCode(xrtGetError()) == w->Index);
        assert(strcmp(xac_last_error(), "thread-local marker") == 0);
        xrtClearError();
    }
    xrtAhoDestroy(w->Automaton);
    return 0;
}

int main(void)
{
    static const xstrview patterns[] = { XRT_STR_INIT("he"), XRT_STR_INIT("she"), XRT_STR_INIT("hers") };
    xrtaho* ac = xrtAhoCreate(NULL);
    xthread* threads[4];
    worker workers[4];
    xatomic32 ready, start;
    xrtAtomic32Init(&ready, 0u);
    xrtAtomic32Init(&start, 0u);
    assert(ac && xrtAhoAddMany(ac, patterns, 3) && xrtAhoBuild(ac));
    for (int i = 0; i < 4; ++i) {
        workers[i] = (worker){ xrtAhoRef(ac), i + 1, &ready, &start };
        threads[i] = xrtThreadCreate(run_worker, &workers[i], 0);
        assert(threads[i]);
    }
    while (xrtAtomic32Load(&ready, XMEMORY_ACQUIRE) != 4u) { xrtThreadYield(); }
    xrtAhoDestroy(ac);
    xrtSetErrorInfo(XERR_RANGE, "aho.thread.test", 99, "parent marker");
    xrtAtomic32Store(&start, 1u, XMEMORY_RELEASE);
    for (int i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    assert(xrtErrorCode(xrtGetError()) == 99);
    xrtClearError();
    puts("aho threads: shared immutable automaton, 1000 independent scans and error isolation passed");
    return 0;
}
