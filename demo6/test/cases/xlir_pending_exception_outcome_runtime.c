#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void pending_exception_case(int, int);
static int faults;
void pending_exception_oom(void)
{
    xerror* error;
    assert(xrtMemDebugFailAfter(0));
    error = xrtErrorCreate(XERR_STATE, "pending-exception", 0, "diagnostic allocation failed");
    assert(error == NULL && xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++faults;
}
int main(int argc, char** argv)
{
    int iteration, probe, fault, cases = 0, argument;
    int smoke = 0, routes = 0;
    xmemdebugsnapshot before, after;
    for (argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--smoke") == 0) smoke = 1;
        else if (strcmp(argv[argument], "--routes") == 0) routes = 1;
        else { fprintf(stderr, "unknown pending-exception probe option: %s\n", argv[argument]); return 2; }
    }
    assert(smoke || xrtMemDebugEnable(true));
    pending_exception_case(routes ? 1 : 0, 0);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration)
        for (probe = 0; probe < 12; ++probe)
            for (fault = 0; fault < 6; ++fault) {
                if (routes && probe != 1 && probe != 2 && probe != 6 && probe < 9) continue;
                if (smoke && fault == 3) continue;
                pending_exception_case(probe, fault); ++cases;
            }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount && faults == (routes ? 600 : 1300));
        printf("pending exception %s: %d cases, %d real diagnostic allocation failures, %llu balanced allocations\n",
            routes ? "routes" : "full", cases, faults, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("pending exception %s smoke: %d cases passed\n", routes ? "routes" : "full", cases);
    return 0;
}
