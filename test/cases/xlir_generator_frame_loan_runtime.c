#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int generator_loan_init(void);
extern void generator_loan_fini(void);
extern void generator_loan_cycle(int, int, int);
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    unsigned round, kind, stop, mode, executions = 0;
    xmemdebugsnapshot initial, before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    assert(generator_loan_init());
    for (round = 0; round < (smoke ? 1u : 100u); ++round)
    for (kind = 0; kind < 3; ++kind)
    for (stop = 0; stop < 4; ++stop)
    for (mode = 0; mode < 3; ++mode) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        generator_loan_cycle((int)kind, (int)stop, (int)mode); ++executions;
        if (!smoke) {
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
            assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        }
    }
    generator_loan_fini(); assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(after.AllocCount-initial.AllocCount == after.FreeCount-initial.FreeCount);
        printf("generator frame loan: %u executions, %llu balanced allocations; private string/object observations, shared updates, natural completion/early close, destructor errors and finally\n",
            executions, (unsigned long long)(after.AllocCount-initial.AllocCount));
    } else printf("generator frame loan C-host smoke: %u executions\n", executions);
    return 0;
}
