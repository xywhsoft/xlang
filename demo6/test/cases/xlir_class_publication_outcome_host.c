#include <__xl_runtime/xruntime_decl.h>
#include <assert.h>
#include <stdio.h>
extern int xl_script_main_checked(xvalue*, int*);
extern int xl_script_module_unit_checked(void);
int main(void)
{
    xmemdebugsnapshot before, after; int code = -1;
    assert(xrtMemDebugEnable(true)); xrtMemDebugSnapshot(&before);
    assert(xl_script_main_checked(NULL, &code) && code == 0);
    assert(xl_script_module_unit_checked() && !xrtGetError());
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount);
    puts("Generated class publication: 100 ordinary successes, 100 native-cursor commits and 100 constructor failures after self publication, catch/local/child/finalizer and allocator balance");
    assert(xrtMemDebugReset()); return 0;
}
