#include "src/compile/xl6_xrt_trim.c"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    XL6_XrtTrimConfig cfg = {0};
    xl6XrtTrimScanGeneratedSource(&cfg, "int main(void) { return 0; }");
    assert(!cfg.keepCoroutine && !cfg.keepValue && !cfg.full);
    const char* calls[] = {"xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE)",
        "xl6PayloadTraceGet(XL6_FUTURE_PAYLOAD_FUTURE)",
        "xl6PayloadResolveOwned(promise, value, XL6_FUTURE_PAYLOAD_XVALUE)",
        "xl6FutureWhenAnyIndex(futures, count)", "xl6FutureAllXValues(futures, count, box)",
        "xl6FutureRaceChecked(futures, count)", "xl6FutureAllChecked(futures, count)",
        "xl6PayloadObserveOutcome(state, error)",
        "(xl6FutureAggregateRuntimeV1Get()->All)(futures, count, box, owner)",
        "(xl6FutureAggregateRuntimeV1Get()->AllArray)(array, box, owner)",
        "(xl6FutureAggregateRuntimeV1Get()->Race)(futures, count)",
        "(xl6FutureAggregateRuntimeV1Get()->RaceArray)(array)",
        "(xl6FutureAggregateRuntimeV1Get()->WhenAnyIndex)(futures, count)",
        "(xl6FutureAggregateRuntimeV1Get()->WhenAnyIndexArray)(array)"};
    for (unsigned i = 0; i < sizeof(calls) / sizeof(calls[0]); ++i) {
        memset(&cfg, 0, sizeof(cfg));
        xl6XrtTrimScanGeneratedSource(&cfg, calls[i]);
        assert(cfg.keepCoroutine && cfg.keepValue && !cfg.full);
        assert(!cfg.keepThread && !cfg.keepCond && !cfg.keepMutex);
    }
    puts("Payload private-source trim: nested drop dependencies retained, no full runtime or executor forced");
    return 0;
}
