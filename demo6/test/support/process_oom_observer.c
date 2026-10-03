/* Preinclude the actual emitted XRT implementation (or the full source runtime).
 * GNU --wrap only observes successful resident -> native Spawn calls. No fake
 * child, allocator, stream, status or wait result is installed. The extra real
 * native reference lets the test join the private waiter tail after module fini;
 * it is NOT an extra guarantee of the public Process.wait contract. */
#include <assert.h>
#include <stdio.h>
#if !defined(XRT_FEATURE_PROCESS)
#error Process OOM observer requires the actual native Process implementation
#endif

static xprocess* observed[8];
static size_t observedCount;
extern xprocess* __real_xrtProcessSpawn(const xprocessconfig* config);
xprocess* __wrap_xrtProcessSpawn(const xprocessconfig* config)
{
    xprocess* result=__real_xrtProcessSpawn(config);
    if(result) {
        assert(observedCount < sizeof(observed)/sizeof(*observed));
        observed[observedCount]=xrtProcessRef(result);
        assert(observed[observedCount]);
        ++observedCount;
    }
    return result;
}
size_t testProcessRetireObserved(void)
{
    size_t count=observedCount;
    for(size_t i=0;i<count;++i) {
        /* A failed language construction may leave an echo child waiting for
         * EOF. The test's private reference closes that real pipe, not a kill
         * or a synthesized completion. All successful fixtures close stdin. */
        assert(xrtProcessClose(observed[i],XPROCESS_STDIN));
        assert(xrtProcessWait(observed[i])==XWAIT_OK);
        assert(xrtThreadWait(observed[i]->Waiter)==XWAIT_OK);
        xrtProcessDestroy(observed[i]); observed[i]=NULL;
    }
    observedCount=0;
    return count;
}
