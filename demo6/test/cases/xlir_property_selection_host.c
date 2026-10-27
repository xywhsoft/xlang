/* Generated C is preincluded. A module fragment executes its top-level body
 * at initialization; a language main uses the public checked entry instead. */
#include <assert.h>
#include <stdio.h>
static void runSelection(xvalue* arguments)
{
#ifdef XL6_PROPERTY_MODULE_FRAGMENT
    (void)arguments;
    assert(xlir_module_init() && !xrtGetError());
    xlir_module_fini();
    assert(!xrtGetError());
#else
    int code = -1;
    if (!xl_script_main_checked(arguments, &code)) xl_script_print_uncaught();
    assert(code == 0 && !xl_script_has_error());
    assert(xl_script_module_unit_checked());
    xl_script_error_unit();
#endif
}
int main(void)
{
    xmemdebugsnapshot before, after;
    assert(xrtMemDebugEnable(true));
    xvalue* arguments = xrtValueArray();
    assert(arguments);
    runSelection(arguments);
    xrtMemDebugSnapshot(&before);
    for (unsigned i = 0; i < 3; ++i) runSelection(arguments);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    xrtValueRelease(arguments);
    puts("property selection: three complete module lifetimes balanced");
    return 0;
}
