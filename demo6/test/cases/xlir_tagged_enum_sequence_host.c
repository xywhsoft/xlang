/* Generated C is preincluded by the ordinary native validation command. */
#include <assert.h>
#include <stdio.h>
static void run(xvalue* arguments)
{
    int code = -1;
    if (!xl_script_main_checked(arguments, &code)) xl_script_print_uncaught();
    assert(code == 0 && !xl_script_has_error());
    assert(xl_script_module_unit_checked());
    xl_script_error_unit();
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xvalue* arguments = xrtValueArray(); assert(arguments);
    run(arguments);
    xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
    for (unsigned i = 0; i < 3; ++i) run(arguments);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount &&
        before.InvalidFreeCount == after.InvalidFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount);
    xrtValueRelease(arguments);
    puts("enum/Stack sequences: three complete module lifetimes balanced");
    return 0;
}
