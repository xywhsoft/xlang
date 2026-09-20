#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern void generator_item_basic_prepare(void);
extern void generator_item_basic_case(void);
extern void generator_item_basic_finish(void);

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount);
    assert(before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}

int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before;
    int i, smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    generator_item_basic_prepare();
    for (i = 0; i < (smoke ? 1 : 100); ++i) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        generator_item_basic_case();
        if (!smoke) balanced(&before);
    }
    generator_item_basic_finish();
    if (!smoke) balanced(&initial);
    printf("generator item basic: %d executions passed; memory checks %s\n",
        i, smoke ? "disabled" : "balanced");
    return 0;
}
