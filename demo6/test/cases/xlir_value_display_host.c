/* Preinclude generated display/OOM C. Check complete, repeated module lifetime
 * after one warm-up, so process-wide allocator caches are outside the sample. */
#include <assert.h>
#include <stdio.h>
static void runDisplay(xvalue* arguments)
{
    int code = -1;
    if (!xl_script_main_checked(arguments, &code)) xl_script_print_uncaught();
    assert(code == 0 && !xl_script_has_error());
    assert(xl_script_module_unit_checked());
    xl_script_error_unit();
}
int main(void)
{
    xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true));
    xvalue* arguments = xrtValueArray();
    assert(arguments != NULL);
    runDisplay(arguments);
    xrtMemDebugSnapshot(&before);
    for (unsigned round = 0; round < 3; ++round) runDisplay(arguments);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    xrtValueRelease(arguments);
    puts("native value display: three complete module lifetimes balanced");
    return 0;
}
