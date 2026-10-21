#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#include "graph_test.h"

typedef struct {
    xvalue* Graph;
    xatomic32* Start;
    int Id;
} worker;

static int run(void* data)
{
    worker* w = data;
    while (!xrtAtomic32Load(w->Start, XMEMORY_ACQUIRE)) xrtThreadYield();
    for (int i = 0; i < 250; ++i) {
        xvalue* result = xg_scc(w->Graph);
        assert(result && xrtValueCount(result) == 1);
        assert(xrtValueCount(xrtValueArrayAt(result, 0)) == 128);
        xrtValueRelease(result);
        result = xg_dijkstra(w->Graph, "0", 1);
        assert(result && xrtValueCount(result) == 128);
        xrtValueRelease(result);
        assert(!xg_is_dag(w->Graph) && !xrtGetError());
        assert(!xg_bfs(w->Graph, "missing", 7));
        assert(xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtSetErrorInfo(XERR_STATE, "graph.thread", w->Id, "thread marker");
        xrtThreadYield();
        assert(xrtErrorCode(xrtGetError()) == w->Id);
        xrtClearError();
    }
    return 0;
}

int main(void)
{
    xvalue* graph = xrtValueArray();
    xthread* threads[4];
    worker workers[4];
    xatomic32 start;
    assert(graph);
    for (int i = 0; i < 128; ++i) {
        char a[16], b[16];
        snprintf(a, sizeof(a), "%d", i);
        snprintf(b, sizeof(b), "%d", (i + 1) % 128);
        edge(graph, a, b, 1);
    }
    xrtAtomic32Init(&start, 0);
    for (int i = 0; i < 4; ++i) {
        workers[i] = (worker){graph, &start, i + 1};
        threads[i] = xrtThreadCreate(run, &workers[i], 0);
        assert(threads[i]);
    }
    xrtSetErrorInfo(XERR_RANGE, "graph.thread", 99, "parent marker");
    xrtAtomic32Store(&start, 1, XMEMORY_RELEASE);
    for (int i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    assert(xrtErrorCode(xrtGetError()) == 99);
    xrtClearError();
    xrtValueRelease(graph);
    puts("graph threads: 4 workers, 1000 rounds, shared immutable input and TLS PASS");
    return 0;
}
