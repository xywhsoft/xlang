#include "graph_test.h"

int main(void)
{
    unsigned rejected = 0;
    xmemdebugsnapshot initial;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&initial);
    for (int invalid = 0; invalid < 2; ++invalid) {
        xvalue* g = xrtValueArray();
        edge(g, "a", "b", 1);
        edge(g, "b", "c", 2);
        edge(g, "z", NULL, 0);
        if (invalid) assert(xrtValueArrayAppendNew(g, xrtValueInt(7)));
        for (int operation = 0; operation < 9; ++operation) {
            xmemdebugsnapshot before;
            int successes = 0;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            for (int i = 0; i < 1024 && successes < 2; ++i) {
                xvalue* result = NULL;
                bool dag = false, failed;
                assert(xrtMemDebugFailAfter(i));
                if (!operation) dag = xg_is_dag(g);
                else result = invoke(operation, g);
                failed = xrtMemDebugFailTriggered();
                xrtMemDebugFailClear();
                if (failed) {
                    assert(!result && !dag && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                    ++rejected;
                } else if (invalid) {
                    assert(!result && !dag && xrtErrorKind(xrtGetError()) == XERR_VALUE);
                    ++successes;
                } else {
                    assert((operation ? result != NULL : dag) && !xrtGetError());
                    ++successes;
                }
                xrtValueRelease(result);
                balanced(&before);
            }
            assert(successes == 2);
        }
        xrtValueRelease(g);
    }
    balanced(&initial);
    printf("graph allocation: %u failure points, all 9 APIs, success/error cleanup PASS\n", rejected);
    return 0;
}
